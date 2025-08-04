#pragma once

#include <json.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <string>
#include <mutex>
#include <condition_variable>
#include "../base/exchange.hpp"

typedef websocketpp::client<websocketpp::config::asio_tls_client> WebSocketClient;
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;

class Deribit : public Exchange
{
public:
    Deribit(const nlohmann::json &config);

    void authenticate() override;
    void fetch_markets() override;
    void fetch_balance() override;
    void fetch_ticker(const std::string &symbol) override;
    void fetch_order_book(const std::string &symbol) override;
    void fetch_orders(const std::string &currency = "BTC") override;
    void create_order(const std::string &symbol, const std::string &side, double amount, double price) override;
    void cancel_order(const std::string &order_id) override;

private:
    bool is_test;
    std::string url;
    WebSocketClient client;
    websocketpp::connection_hdl connection_hdl;
    bool connected = false;
    std::mutex mtx;
    std::condition_variable cv;
    int request_id = 1;

    void connect();
    void on_open(websocketpp::connection_hdl);
    void on_message(websocketpp::connection_hdl, message_ptr msg);
    void on_fail(websocketpp::connection_hdl);
    void on_close(websocketpp::connection_hdl);
    void send_request(const nlohmann::json &request);
    bool is_connected();
};
