#pragma once

#include <json.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <string>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include "../base/exchange.hpp"

typedef websocketpp::client<websocketpp::config::asio_tls_client> WebSocketClient;
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;

struct ResponseHandler
{
    std::mutex mtx;
    std::condition_variable cv;
    nlohmann::json response;
    bool received = false;
};

class Deribit : public Exchange
{
public:
    Deribit(const nlohmann::json &config);
    ~Deribit();

    nlohmann::json markets;
    std::unordered_map<std::string, nlohmann::json> markets_by_id;

    void authenticate() override;

    nlohmann::json load_markets(bool reload = false, const nlohmann::json &params = nlohmann::json::object()) override;
    nlohmann::json fetch_markets(const nlohmann::json &params = nlohmann::json::object()) override;
    nlohmann::json fetch_balance(const nlohmann::json &params = nlohmann::json::object()) override;
    nlohmann::json fetch_ticker(const std::string &symbol) override;
    nlohmann::json fetch_order_book(const std::string &symbol, const nlohmann::json &params = nlohmann::json::object()) override;
    nlohmann::json fetch_orders(const std::string &symbol = "", const std::string &currency = "any", const std::string &kind = "any", const std::string &interval = "raw", const nlohmann::json &extra_params = {}) override;

    nlohmann::json create_order(const std::string &symbol, const std::string &type, const std::string &side, double amount, std::optional<double> price = std::nullopt, const nlohmann::json &params = nlohmann::json::object()) override;

    nlohmann::json cancel_order(const std::string &id, const std::string &symbol = "", const nlohmann::json &params = nlohmann::json::object()) override;

private:
    bool is_test;
    std::string url;
    WebSocketClient client;
    websocketpp::connection_hdl connection_hdl;
    bool connected = false;
    bool connection_failed = false;
    std::mutex mtx;
    std::condition_variable cv;
    int request_id = 1;

    std::string apiKey;
    std::string secret;
    std::string password;
    std::string access_token;
    long long auth_expires_at = 0;
    bool authenticated = false;
    bool auth_in_progress = false;
    std::mutex auth_mtx;
    std::condition_variable auth_cv;

    std::mutex pending_requests_mutex;
    std::unordered_map<int, ResponseHandler> pending_requests;

    void connect();
    bool is_connected();
    void on_open(websocketpp::connection_hdl);
    void on_fail(websocketpp::connection_hdl);
    void on_close(websocketpp::connection_hdl);
    void send_request(const nlohmann::json &request);
    void on_message(websocketpp::connection_hdl, message_ptr msg);
    std::string generate_signature(const std::string &timestamp, const std::string &nonce);
    nlohmann::json send_request_and_wait(const nlohmann::json &request, int timeout_seconds = 30);
};
