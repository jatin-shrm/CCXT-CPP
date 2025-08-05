#include "include/deribit.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>

Deribit::Deribit(const nlohmann::json &config)
{
    apiKey = config.value("apiKey", "");
    secret = config.value("secret", "");
    password = config.value("password", "");
    is_test = config.value("is_test", true);
    url = is_test ? "wss://test.deribit.com/ws/api/v2" : "wss://www.deribit.com/ws/api/v2";

    client.init_asio();
    client.set_open_handler(std::bind(&Deribit::on_open, this, std::placeholders::_1));
    client.set_message_handler(std::bind(&Deribit::on_message, this, std::placeholders::_1, std::placeholders::_2));
    client.set_fail_handler(std::bind(&Deribit::on_fail, this, std::placeholders::_1));
    client.set_close_handler(std::bind(&Deribit::on_close, this, std::placeholders::_1));
}

void Deribit::connect()
{
    client.set_tls_init_handler([](websocketpp::connection_hdl)
                                { return websocketpp::lib::make_shared<boost::asio::ssl::context>(
                                      boost::asio::ssl::context::tlsv12_client); });
    websocketpp::lib::error_code ec;
    auto con = client.get_connection(url, ec);
    if (ec)
    {
        std::cerr << "Connection error: " << ec.message() << std::endl;
        return;
    }

    client.connect(con);
    std::thread([this]()
                { client.run(); })
        .detach();

    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, [this]()
            { return connected; });
}

bool Deribit::is_connected()
{
    return connected;
}

void Deribit::on_open(websocketpp::connection_hdl hdl)
{
    connection_hdl = hdl;
    {
        std::lock_guard<std::mutex> lock(mtx);
        connected = true;
    }
    cv.notify_one();
}

void Deribit::on_message(websocketpp::connection_hdl, message_ptr msg)
{
    std::string payload = msg->get_payload();
    std::cout << "Received: " << payload << std::endl;
    try
    {
        auto response = nlohmann::json::parse(payload);

        if (response.contains("id") && response["id"] == last_auth_id)
        {
            if (response.contains("result") && response["result"].contains("access_token"))
            {
                access_token = response["result"]["access_token"];
                long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::system_clock::now().time_since_epoch())
                                    .count();
                auth_expires_at = now + response["result"].value("expires_in", 0LL) * 1000;

                std::lock_guard<std::mutex> lock(auth_mtx);
                authenticated = true;
                auth_in_progress = false;
                auth_cv.notify_all();
                return;
            }
        }

        if (response.contains("params") && response["params"].contains("channel"))
        {
            std::string channel = response["params"]["channel"];

            if (channel.rfind("user.orders", 0) == 0) // starts with "user.orders"
            {
                std::cout << "[ORDER UPDATE]:\n"
                          << response["params"]["data"].dump(4) << "\n";
            }
            else
            {
                std::cout << "[CHANNEL MESSAGE] " << channel << ":\n"
                          << response["params"].dump(4) << "\n";
            }
        }

        // Handle other responses...
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to parse message: " << e.what() << std::endl;
    }
}

void Deribit::on_fail(websocketpp::connection_hdl)
{
    std::cerr << "Connection failed\n";
}

void Deribit::on_close(websocketpp::connection_hdl)
{
    std::cerr << "Connection closed\n";
    connected = false;
}

void Deribit::send_request(const nlohmann::json &request)
{
    if (!is_connected())
    {
        connect();
    }

    websocketpp::lib::error_code ec;
    std::string payload = request.dump();
    client.send(connection_hdl, payload, websocketpp::frame::opcode::text, ec);
    if (ec)
    {
        std::cerr << "Send failed: " << ec.message() << std::endl;
    }
}

void Deribit::fetch_markets()
{
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/get_instruments"},
        {"params", {{"currency", "BTC"}, {"kind", "future"}}}};
    send_request(req);
}

void Deribit::authenticate()
{
    std::unique_lock<std::mutex> lock(auth_mtx);

    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

    if (authenticated && now < auth_expires_at)
    {
        return;
    }

    if (auth_in_progress)
    {
        auth_cv.wait(lock, [this]()
                     { return authenticated; });
        return;
    }

    auth_in_progress = true;

    if (!is_connected())
    {
        connect();
    }

    // 1. Timestamp and nonce
    std::string timestamp = std::to_string(now);
    std::string nonce = timestamp;

    // 2. Message to sign
    std::string message = timestamp + "\n" + nonce + "\n";

    // 3. HMAC-SHA256 signature
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = EVP_MAX_MD_SIZE;

    HMAC(EVP_sha256(),
         secret.c_str(), secret.length(),
         reinterpret_cast<const unsigned char *>(message.c_str()), message.length(),
         hash, &len);

    // 4. Convert to hex lowercase string
    std::stringstream ss;
    for (unsigned int i = 0; i < len; ++i)
    {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
    }
    std::string signature = ss.str();

    last_auth_id = request_id++;

    // 5. Send the authentication request
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", last_auth_id},
        {"method", "public/auth"},
        {"params", {{"grant_type", "client_signature"}, {"client_id", apiKey}, {"timestamp", now}, {"nonce", nonce}, {"signature", signature}, {"data", ""}}}};

    send_request(req);

    // 6. Wait until on_message() confirms authentication
    auth_cv.wait(lock, [this]()
                 { return authenticated; });
}

void Deribit::fetch_balance()
{

    authenticate();

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/get_account_summary"},
        {"params", {{"currency", "BTC"}}}};

    send_request(req);
}


void Deribit::fetch_orders(const std::string &symbol,
                           const std::string &currency,
                           const std::string &kind,
                           const std::string &interval,
                           const nlohmann::json &extra_params)
{
    authenticate(); // Assume this ensures the access token is valid

    std::string channel = "user.orders." + kind + "." + currency + "." + interval;

    nlohmann::json params = {
        {"channels", {channel}}};

    // Merge extra_params into the 'params' if needed
    for (auto it = extra_params.begin(); it != extra_params.end(); ++it)
    {
        params[it.key()] = it.value();
    }

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"method", "private/subscribe"},
        {"id", request_id++},
        {"params", params}};

    send_request(req); // This sends the message over the WebSocket
}

void Deribit::fetch_ticker(const std::string &symbol) {}
void Deribit::fetch_order_book(const std::string &symbol) {}
void Deribit::create_order(const std::string &symbol, const std::string &side, double amount, double price)
{
    authenticate();

    std::string method = (side == "buy") ? "private/buy" : "private/sell";

    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", method},
        {"params", {{"instrument_name", symbol}, {"amount", amount}, {"type", "limit"}, // or "market"
                    {"price", price}}}};

    send_request(request);
}
void Deribit::cancel_order(const std::string &order_id)
{
    authenticate();

    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/cancel"},
        {"params", {{"order_id", order_id}}}};

    send_request(request);
}
