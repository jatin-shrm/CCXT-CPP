#pragma once

#include <json.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <string>
#include <mutex>
#include <condition_variable>
#include <future>
#include <unordered_map>
#include "../base/exchange.hpp"

typedef websocketpp::client<websocketpp::config::asio_tls_client> WebSocketClient;
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;

class Deribit : public Exchange
{
public:
    Deribit(const nlohmann::json &config);

    std::future<nlohmann::json> send_async_request(const std::string &method, const nlohmann::json &params);
    std::future<nlohmann::json> authenticate() override;
    std::future<nlohmann::json> fetch_markets() override;
    std::future<nlohmann::json> fetch_balance() override;
    std::future<nlohmann::json> fetch_ticker(const std::string &symbol) override;
    std::future<nlohmann::json> fetch_order_book(const std::string &symbol) override;
    std::future<nlohmann::json> fetch_orders(const std::string &symbol = "",
                                             const std::string &currency = "any",
                                             const std::string &kind = "any",
                                             const std::string &interval = "raw",
                                             const nlohmann::json &extra_params = {}) override;

    std::future<nlohmann::json> create_order(
        const std::string &symbol,
        const std::string &type,
        const std::string &side,
        double amount,
        std::optional<double> price = std::nullopt,
        const nlohmann::json &params = nlohmann::json::object()) override;

    std::future<nlohmann::json> cancel_order(const std::string &order_id) override;

private:
    bool is_test;
    std::string url;
    WebSocketClient client;
    websocketpp::connection_hdl connection_hdl;
    bool connected = false;
    std::mutex mtx;
    std::condition_variable cv;
    int request_id = 1;

    std::string access_token = "";
    long long auth_expires_at = 0;

    // Authentication tracking
    bool authenticated = false;
    bool auth_in_progress = false;
    int last_auth_id = -1;
    std::mutex auth_mtx;
    std::condition_variable auth_cv;

    // Request tracking for async
    std::mutex response_mtx;
    std::unordered_map<int, std::promise<nlohmann::json>> pending_requests;
    std::unordered_map<int, std::promise<nlohmann::json>> promises;
    std::mutex promise_mtx;

    void connect();
    void on_open(websocketpp::connection_hdl);
    void on_message(websocketpp::connection_hdl, message_ptr msg);
    void on_fail(websocketpp::connection_hdl);
    void on_close(websocketpp::connection_hdl);
    std::future<nlohmann::json> send_request(const nlohmann::json &request);
    bool is_connected();
};
