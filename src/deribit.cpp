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

std::future<nlohmann::json> Deribit::send_async_request(const std::string &method, const nlohmann::json &params)
{
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", method},
        {"params", params}};

    return send_request(request);
}

void Deribit::on_fail(websocketpp::connection_hdl)
{
    std::cerr << "Connection failed\n";
    connected = false;
}

void Deribit::on_close(websocketpp::connection_hdl)
{
    std::cerr << "Connection closed\n";
    connected = false;
}

std::future<nlohmann::json> Deribit::send_request(const nlohmann::json &request)
{
    if (!is_connected())
    {
        connect();
    }

    int id = request["id"].get<int>();
    std::promise<nlohmann::json> prom;
    auto fut = prom.get_future();

    {
        std::lock_guard<std::mutex> lock(promise_mtx);
        promises[id] = std::move(prom);
    }

    websocketpp::lib::error_code ec;
    client.send(connection_hdl, request.dump(), websocketpp::frame::opcode::text, ec);

    if (ec)
    {
        throw std::runtime_error("Send failed: " + ec.message());
    }

    return fut;
}

std::future<nlohmann::json> Deribit::fetch_markets()
{
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/get_instruments"},
        {"params", {{"currency", "BTC"}, {"kind", "spot"}, {"expired", false}}}};
    return send_request(req);
}

std::future<nlohmann::json> Deribit::authenticate()
{
    std::unique_lock<std::mutex> lock(auth_mtx);
    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

    if (authenticated && now < auth_expires_at)
    {
        std::promise<nlohmann::json> prom;
        prom.set_value({{"status", "already_authenticated"}});
        return prom.get_future();
    }

    if (auth_in_progress)
    {
        auth_cv.wait(lock, [this]() { return authenticated; });
        std::promise<nlohmann::json> prom;
        prom.set_value({{"status", "waited_auth"}});
        return prom.get_future();
    }

    auth_in_progress = true;

    std::string timestamp = std::to_string(now);
    std::string nonce = timestamp;
    std::string message = timestamp + "\n" + nonce + "\n";

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = EVP_MAX_MD_SIZE;

    HMAC(EVP_sha256(),
         secret.c_str(), secret.length(),
         reinterpret_cast<const unsigned char *>(message.c_str()), message.length(),
         hash, &len);

    std::stringstream ss;
    for (unsigned int i = 0; i < len; ++i)
    {
        ss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
    }

    std::string signature = ss.str();
    last_auth_id = request_id++;

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", last_auth_id},
        {"method", "public/auth"},
        {"params", {{"grant_type", "client_signature"},
                     {"client_id", apiKey},
                     {"timestamp", now},
                     {"nonce", nonce},
                     {"signature", signature},
                     {"data", ""}}}};

    auto fut = send_request(req);
    fut.wait();
    auto response = fut.get();

    if (response.contains("result") && response["result"].contains("access_token"))
    {
        access_token = response["result"]["access_token"];
        auth_expires_at = now + response["result"].value("expires_in", 0LL) * 1000;
        authenticated = true;
    }
    auth_in_progress = false;
    auth_cv.notify_all();

    return std::async(std::launch::deferred, [response]() { return response; });
}

std::future<nlohmann::json> Deribit::fetch_balance()
{
    authenticate();

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/get_account_summary"},
        {"params", {{"currency", "BTC"}}}};

    return send_async_request(req);
}

std::future<nlohmann::json> Deribit::fetch_orders(const std::string &currency,
                                                  const std::string &kind,
                                                  const std::string &order_type,
                                                  const std::string &extra_state,
                                                  const nlohmann::json &extra_params)
{
    authenticate(); // Ensure the token is valid

    nlohmann::json params = {
        {"currency", currency},
        {"kind", kind} // "option" or "future"
    };

    // Optionally add order_type or state filters
    if (!order_type.empty())
        params["type"] = order_type;
    if (!extra_state.empty())
        params["state"] = extra_state;

    // Merge extra_params
    for (auto it = extra_params.begin(); it != extra_params.end(); ++it)
        params[it.key()] = it.value();

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/get_order_history_by_currency"},
        {"params", params}};

    return send_async_request(req);
}

std::future<nlohmann::json> fetch_ticker(const std::string &symbol) {};
std::future<nlohmann::json> fetch_order_book(const std::string &symbol) {};

std::future<nlohmann::json> Deribit::create_order(
    const std::string &symbol,
    const std::string &type,
    const std::string &side,
    double amount,
    std::optional<double> price,
    const nlohmann::json &params)
{
    authenticate();

    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = request_id++;
    request["method"] = side == "buy" ? "private/buy" : "private/sell";

    nlohmann::json order_params;
    order_params["instrument_name"] = symbol;
    order_params["amount"] = amount;

    std::string order_type = type;
    std::string trigger = params.value("trigger", "last_price");
    std::string timeInForce = params.value("timeInForce", "");
    bool reduceOnly = params.value("reduceOnly", false);
    bool postOnly = params.value("postOnly", false);

    auto trailingAmountIt = params.find("trailingAmount");
    bool isTrailingAmount = trailingAmountIt != params.end() && !trailingAmountIt->is_null();

    auto stopLossPriceIt = params.find("stopLossPrice");
    auto takeProfitPriceIt = params.find("takeProfitPrice");
    bool hasStopLoss = stopLossPriceIt != params.end() && !stopLossPriceIt->is_null();
    bool hasTakeProfit = takeProfitPriceIt != params.end() && !takeProfitPriceIt->is_null();

    if (hasStopLoss && hasTakeProfit)
    {
        throw std::runtime_error("Cannot specify both stopLossPrice and takeProfitPrice.");
    }

    bool isLimit = (type == "limit");
    bool isMarket = (type == "market");

    if (isLimit && price.has_value())
    {
        order_params["type"] = "limit";
        order_params["price"] = price.value();
    }
    else if (isMarket)
    {
        order_params["type"] = "market";
    }

    if (isTrailingAmount)
    {
        order_params["type"] = "trailing_stop";
        order_params["trigger"] = trigger;
        order_params["trigger_offset"] = std::stod(trailingAmountIt->get<std::string>());
    }
    else if (hasStopLoss || hasTakeProfit)
    {
        double triggerPrice = hasStopLoss ? stopLossPriceIt->get<double>() : takeProfitPriceIt->get<double>();
        order_params["trigger"] = trigger;
        order_params["trigger_price"] = triggerPrice;

        if (hasStopLoss)
        {
            order_params["type"] = isMarket ? "stop_market" : "stop_limit";
        }
        else
        {
            order_params["type"] = isMarket ? "take_market" : "take_limit";
        }
    }

    if (reduceOnly)
    {
        order_params["reduce_only"] = true;
    }
    if (postOnly)
    {
        order_params["post_only"] = true;
        order_params["reject_post_only"] = true;
    }
    if (!timeInForce.empty())
    {
        if (timeInForce == "GTC")
            order_params["time_in_force"] = "good_til_cancelled";
        else if (timeInForce == "IOC")
            order_params["time_in_force"] = "immediate_or_cancel";
        else if (timeInForce == "FOK")
            order_params["time_in_force"] = "fill_or_kill";
    }

    request["params"] = order_params;

    return send_async_request(request);
}

std::future<nlohmann::json> Deribit::cancel_order(const std::string &order_id)
{
    authenticate();

    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/cancel"},
        {"params", {{"order_id", order_id}}}};

    return send_async_request(request);
}
