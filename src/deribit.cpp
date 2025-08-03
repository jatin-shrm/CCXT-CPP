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
    std::cout << "Received: " << msg->get_payload() << std::endl;
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
    if (!is_connected())
    {
        connect();
    }

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/auth"},
        {"params", {{"grant_type", "client_credentials"}, {"client_id", apiKey}, {"client_secret", secret}}}};

    send_request(req);
}

void Deribit::fetch_balance() {
    authenticate();                                              // Step 1: Auth
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // small wait for auth

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/get_account_summary"},
        {"params", {{"currency", "BTC"}}}};

    send_request(req);
}
void Deribit::fetch_ticker(const std::string &symbol) {}
void Deribit::fetch_order_book(const std::string &symbol) {}
void Deribit::create_order(const std::string &symbol, const std::string &side, double amount, double price) {}
void Deribit::cancel_order(const std::string &order_id) {}
