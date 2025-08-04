#pragma once

#include "../../base/exchange.hpp"
#include "types.hpp"
#include "exceptions.hpp"
#include "utils.hpp"
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <future>
#include <unordered_map>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>

namespace ccxt {

typedef websocketpp::client<websocketpp::config::asio_tls_client> WebSocketClient;
typedef websocketpp::config::asio_tls_client::message_type::ptr message_ptr;

class DeribitImproved : public Exchange {
public:
    explicit DeribitImproved(const nlohmann::json& config = {});
    ~DeribitImproved();
    
    // Exchange interface implementation
    std::future<std::vector<Market>> fetch_markets() override;
    std::future<Balance> fetch_balance(const std::string& currency = "BTC") override;
    std::future<Ticker> fetch_ticker(const std::string& symbol) override;
    std::future<OrderBook> fetch_order_book(const std::string& symbol, int limit = 100) override;
    std::future<std::vector<Order>> fetch_orders(const std::string& symbol = "", int limit = 100) override;
    std::future<std::vector<Order>> fetch_open_orders(const std::string& symbol = "", int limit = 100) override;
    std::future<std::vector<Order>> fetch_closed_orders(const std::string& symbol = "", int limit = 100) override;
    std::future<Order> fetch_order(const std::string& order_id, const std::string& symbol = "") override;
    std::future<std::vector<Trade>> fetch_my_trades(const std::string& symbol = "", int limit = 100) override;
    
    // Trading operations
    std::future<Order> create_order(const std::string& symbol, const std::string& type,
                                   const std::string& side, double amount, 
                                   double price = 0.0, const std::string& client_order_id = "") override;
    std::future<Order> cancel_order(const std::string& order_id, const std::string& symbol = "") override;
    std::future<std::vector<Order>> cancel_all_orders(const std::string& symbol = "") override;
    
    // Position management
    std::future<std::vector<Position>> fetch_positions(const std::string& symbol = "") override;
    
    // Connection management
    std::future<void> connect() override;
    std::future<void> disconnect() override;
    bool is_connected() const override;
    
    // Exchange information
    std::string get_exchange_name() const override { return "Deribit"; }
    int get_rate_limit() const override { return 20; } // 20 requests per second for Deribit

private:
    // Connection configuration
    std::string websocket_url_;
    WebSocketClient client_;
    websocketpp::connection_hdl connection_hdl_;
    std::thread websocket_thread_;
    
    // Connection state
    std::atomic<bool> connected_{false};
    std::atomic<bool> authenticated_{false};
    std::atomic<bool> should_stop_{false};
    std::condition_variable connection_cv_;
    std::mutex connection_mutex_;
    
    // Request-response correlation
    std::atomic<int> next_request_id_{1};
    std::mutex pending_requests_mutex_;
    std::unordered_map<int, std::promise<nlohmann::json>> pending_requests_;
    
    // Authentication state
    std::string access_token_;
    std::chrono::system_clock::time_point token_expiry_;
    std::mutex auth_mutex_;
    std::promise<void> auth_promise_;
    std::future<void> auth_future_;
    
    // Rate limiting
    std::queue<std::chrono::steady_clock::time_point> request_timestamps_;
    std::mutex rate_limit_mutex_;
    
    // Connection methods
    void websocket_thread_function();
    void setup_websocket_handlers();
    void ensure_connection();
    std::future<void> authenticate_internal();
    void check_rate_limit();
    
    // WebSocket event handlers
    void on_open(websocketpp::connection_hdl hdl);
    void on_message(websocketpp::connection_hdl hdl, message_ptr msg);
    void on_fail(websocketpp::connection_hdl hdl);
    void on_close(websocketpp::connection_hdl hdl);
    
    // Request handling
    std::future<nlohmann::json> send_request(const std::string& method, 
                                            const nlohmann::json& params = {},
                                            bool requires_auth = false);
    void handle_response(const nlohmann::json& response);
    void handle_notification(const nlohmann::json& notification);
    
    // Error handling
    void handle_error_response(const nlohmann::json& error);
    CCXTException create_exception_from_error(const nlohmann::json& error);
    
    // Response parsers
    Balance parse_balance(const nlohmann::json& data);
    Order parse_order(const nlohmann::json& data);
    Trade parse_trade(const nlohmann::json& data);
    Ticker parse_ticker(const nlohmann::json& data);
    OrderBook parse_order_book(const nlohmann::json& data);
    Market parse_market(const nlohmann::json& data);
    Position parse_position(const nlohmann::json& data);
    
    // Utility methods
    std::string get_websocket_url() const;
    OrderType string_to_order_type(const std::string& type_str);
    OrderSide string_to_order_side(const std::string& side_str);
    OrderStatus string_to_order_status(const std::string& status_str);
    std::string normalize_symbol(const std::string& symbol);
    
    // Market data cache
    mutable std::mutex markets_mutex_;
    std::vector<Market> cached_markets_;
    std::chrono::system_clock::time_point markets_cache_time_;
    static constexpr int MARKETS_CACHE_TTL_SECONDS = 300; // 5 minutes
};

} // namespace ccxt