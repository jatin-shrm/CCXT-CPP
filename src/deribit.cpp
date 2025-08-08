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

    client.clear_access_channels(websocketpp::log::alevel::all);
    client.clear_error_channels(websocketpp::log::elevel::all);
    client.init_asio();
    client.set_open_handler(std::bind(&Deribit::on_open, this, std::placeholders::_1));
    client.set_message_handler(std::bind(&Deribit::on_message, this, std::placeholders::_1, std::placeholders::_2));
    client.set_fail_handler(std::bind(&Deribit::on_fail, this, std::placeholders::_1));
    client.set_close_handler(std::bind(&Deribit::on_close, this, std::placeholders::_1));
}

Deribit::~Deribit()
{
    if (connected)
    {
        client.close(connection_hdl, websocketpp::close::status::normal, "");
    }
}

void Deribit::connect()
{
    std::unique_lock<std::mutex> lock(mtx);
    if (connected)
    {
        return;
    }

    connection_failed = false;
    client.set_tls_init_handler([](websocketpp::connection_hdl)
                                { return websocketpp::lib::make_shared<boost::asio::ssl::context>(
                                      boost::asio::ssl::context::tlsv12_client); });
    websocketpp::lib::error_code ec;
    auto con = client.get_connection(url, ec);
    if (ec)
    {
        throw std::runtime_error("Connection error: " + ec.message());
    }

    client.connect(con);
    std::thread([this]()
                { client.run(); })
        .detach();

    if (!cv.wait_for(lock, std::chrono::seconds(10), [this]()
                     { return connected || connection_failed; }))
    {
        throw std::runtime_error("Connection timed out");
    }

    if (connection_failed)
    {
        throw std::runtime_error("Connection failed");
    }
}

bool Deribit::is_connected()
{
    std::lock_guard<std::mutex> lock(mtx);
    return connected;
}

void Deribit::on_open(websocketpp::connection_hdl hdl)
{
    connection_hdl = hdl;
    {
        std::lock_guard<std::mutex> lock(mtx);
        connected = true;
        connection_failed = false;
    }
    cv.notify_one();
}

void Deribit::on_message(websocketpp::connection_hdl, message_ptr msg)
{
    std::string payload = msg->get_payload();
    try
    {
        auto response = nlohmann::json::parse(payload);

        if (response.contains("id") && response["id"].is_number_integer())
        {
            int id = response["id"];
            std::lock_guard<std::mutex> lock(pending_requests_mutex);
            auto it = pending_requests.find(id);
            if (it != pending_requests.end())
            {
                ResponseHandler &handler = it->second;
                std::lock_guard<std::mutex> lock_handler(handler.mtx);
                handler.response = response;
                handler.received = true;
                handler.cv.notify_one();
            }
        }
        else if (response.contains("error"))
        {
            std::cerr << "Deribit error: " << response["error"].dump() << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to parse message: " << e.what() << std::endl;
    }
}

void Deribit::on_fail(websocketpp::connection_hdl)
{
    std::cerr << "Connection failed" << std::endl;
    {
        std::lock_guard<std::mutex> lock(mtx);
        connected = false;
        connection_failed = true;
    }
    cv.notify_one();
}

void Deribit::on_close(websocketpp::connection_hdl)
{
    std::cerr << "Connection closed" << std::endl;
    {
        std::lock_guard<std::mutex> lock(mtx);
        connected = false;
    }
    cv.notify_one();
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
        throw std::runtime_error("Send failed: " + ec.message());
    }
}

nlohmann::json Deribit::send_request_and_wait(const nlohmann::json &request, int timeout_seconds)
{
    int id = request["id"];
    {
        std::lock_guard<std::mutex> lock(pending_requests_mutex);
        pending_requests.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(id),
                                 std::forward_as_tuple());
    }

    try
    {
        send_request(request);
    }
    catch (...)
    {
        std::lock_guard<std::mutex> lock(pending_requests_mutex);
        pending_requests.erase(id);
        throw;
    }

    auto &handler = pending_requests[id];
    std::unique_lock<std::mutex> lock(handler.mtx);
    if (handler.cv.wait_for(lock, std::chrono::seconds(timeout_seconds),
                            [&handler]()
                            { return handler.received; }))
    {
        nlohmann::json resp = handler.response;
        {
            std::lock_guard<std::mutex> lock_map(pending_requests_mutex);
            pending_requests.erase(id);
        }
        return resp;
    }
    else
    {
        std::lock_guard<std::mutex> lock_map(pending_requests_mutex);
        pending_requests.erase(id);
        throw std::runtime_error("Request timed out");
    }
}

std::string Deribit::generate_signature(const std::string &timestamp, const std::string &nonce)
{
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
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return ss.str();
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
        auth_cv.wait(lock, [this, now]()
                     { return authenticated && now < auth_expires_at; });
        return;
    }

    auth_in_progress = true;
    lock.unlock();

    std::string timestamp = std::to_string(now);
    std::string nonce = timestamp;
    std::string signature = generate_signature(timestamp, nonce);

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/auth"},
        {"params", {{"grant_type", "client_signature"}, {"client_id", apiKey}, {"timestamp", now}, {"nonce", nonce}, {"signature", signature}, {"data", ""}}}};

    try
    {
        auto resp = send_request_and_wait(req, 30);
        std::unique_lock<std::mutex> auth_lock(auth_mtx);
        if (resp.contains("result") && resp["result"].is_object())
        {
            auto result = resp["result"];
            if (result.contains("access_token"))
            {
                access_token = result["access_token"];
                auth_expires_at = now + result.value("expires_in", 0) * 1000;
                authenticated = true;
            }
        }
        else if (resp.contains("error"))
        {
            throw std::runtime_error("Authentication failed: " + resp["error"].dump());
        }
        auth_in_progress = false;
        auth_cv.notify_all();
    }
    catch (const std::exception &e)
    {
        std::unique_lock<std::mutex> auth_lock(auth_mtx);
        authenticated = false;
        auth_in_progress = false;
        auth_cv.notify_all();
        throw;
    }
}

nlohmann::json Deribit::fetch_markets()
{
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/get_instruments"},
        {"params", {{"currency", "BTC"}, {"kind", "spot"}, {"expired", false}}}};
    return send_request_and_wait(req, 30);
}

nlohmann::json Deribit::fetch_balance()
{
    authenticate();
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/get_account_summary"},
        {"params", {{"currency", "BTC"}}}};
    return send_request_and_wait(req, 30);
}

nlohmann::json Deribit::fetch_orders(const std::string &symbol, const std::string &currency, const std::string &kind, const std::string &interval, const nlohmann::json &extra_params)
{
    authenticate();
    nlohmann::json params = {
        {"currency", currency},
        {"kind", kind}};

    if (!symbol.empty())
        params["instrument_name"] = symbol;
    for (auto it = extra_params.begin(); it != extra_params.end(); ++it)
    {
        params[it.key()] = it.value();
    }

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/get_order_history_by_currency"},
        {"params", params}};
    return send_request_and_wait(req, 30);
}

nlohmann::json Deribit::fetch_ticker(const std::string &symbol)
{
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/ticker"},
        {"params", {{"instrument_name", symbol}}}};
    return send_request_and_wait(req, 30);
}

nlohmann::json Deribit::fetch_order_book(const std::string &symbol)
{
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/get_order_book"},
        {"params", {{"instrument_name", symbol}, {"depth", 5}}}};
    return send_request_and_wait(req, 30);
}

nlohmann::json Deribit::create_order(const std::string &symbol, const std::string &type, const std::string &side, double amount, std::optional<double> price, const nlohmann::json &params)
{
    authenticate();
    nlohmann::json req;
    req["jsonrpc"] = "2.0";
    req["id"] = request_id++;
    req["method"] = side == "buy" ? "private/buy" : "private/sell";

    nlohmann::json order_params;
    order_params["instrument_name"] = symbol;
    order_params["amount"] = amount;

    std::string trigger = params.value("trigger", "last_price");
    std::string timeInForce = params.value("timeInForce", "");
    bool reduceOnly = params.value("reduceOnly", false);
    bool postOnly = params.value("postOnly", false);
    auto trailingAmountIt = params.find("trailingAmount");
    auto stopLossPriceIt = params.find("stopLossPrice");
    auto takeProfitPriceIt = params.find("takeProfitPrice");
    bool hasStopLoss = stopLossPriceIt != params.end() && !stopLossPriceIt->is_null();
    bool hasTakeProfit = takeProfitPriceIt != params.end() && !takeProfitPriceIt->is_null();

    if (hasStopLoss && hasTakeProfit)
    {
        throw std::runtime_error("Cannot specify both stopLossPrice and takeProfitPrice");
    }

    if (type == "limit" && price.has_value())
    {
        order_params["type"] = "limit";
        order_params["price"] = price.value();
    }
    else if (type == "market")
    {
        order_params["type"] = "market";
    }

    if (trailingAmountIt != params.end() && !trailingAmountIt->is_null())
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
            order_params["type"] = type == "market" ? "stop_market" : "stop_limit";
        }
        else
        {
            order_params["type"] = type == "market" ? "take_market" : "take_limit";
        }
    }

    if (reduceOnly)
        order_params["reduce_only"] = true;
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

    req["params"] = order_params;
    return send_request_and_wait(req, 30);
}

nlohmann::json Deribit::cancel_order(const std::string &order_id)
{
    authenticate();
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/cancel"},
        {"params", {{"order_id", order_id}}}};
    return send_request_and_wait(req, 30);
}