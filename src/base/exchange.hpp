#pragma once

#include <string>
#include <future>
#include <vector>
#include <memory>
#include <json.hpp>
#include "include/types.hpp"
#include "include/exceptions.hpp"
#include "include/utils.hpp"

namespace ccxt {

class Exchange {
private:
    std::string api_key_;
    std::string secret_;
    std::string password_;
    bool sandbox_mode_;
    
protected:
    // Protected getters for derived classes
    const std::string& get_api_key() const { return api_key_; }
    const std::string& get_secret() const { return secret_; }
    const std::string& get_password() const { return password_; }
    bool is_sandbox() const { return sandbox_mode_; }
    
    // Utility functions available to derived classes
    std::string safe_string(const nlohmann::json& obj, const std::string& key, 
                           const std::string& default_val = "") const {
        return Utils::safe_string(obj, key, default_val);
    }
    
    double safe_float(const nlohmann::json& obj, const std::string& key, double default_val = 0.0) const {
        return Utils::safe_float(obj, key, default_val);
    }
    
    int64_t safe_timestamp(const nlohmann::json& obj, const std::string& key, int64_t default_val = 0) const {
        return Utils::safe_timestamp(obj, key, default_val);
    }
    
public:
    // Constructor with secure credential handling
    Exchange(const std::string& api_key = "", const std::string& secret = "", 
             const std::string& password = "", bool sandbox = false)
        : api_key_(api_key), secret_(secret), password_(password), sandbox_mode_(sandbox) {}
    
    virtual ~Exchange() = default;
    
    // Configuration
    virtual void set_credentials(const std::string& api_key, const std::string& secret, 
                                const std::string& password = "") {
        api_key_ = api_key;
        secret_ = secret;
        password_ = password;
    }
    
    virtual void set_sandbox_mode(bool sandbox) {
        sandbox_mode_ = sandbox;
    }
    
    // Pure virtual async interface - all exchanges must implement
    virtual std::future<std::vector<Market>> fetch_markets() = 0;
    virtual std::future<Balance> fetch_balance(const std::string& currency = "") = 0;
    virtual std::future<Ticker> fetch_ticker(const std::string& symbol) = 0;
    virtual std::future<OrderBook> fetch_order_book(const std::string& symbol, int limit = 100) = 0;
    virtual std::future<std::vector<Order>> fetch_orders(const std::string& symbol = "", int limit = 100) = 0;
    virtual std::future<std::vector<Order>> fetch_open_orders(const std::string& symbol = "", int limit = 100) = 0;
    virtual std::future<std::vector<Order>> fetch_closed_orders(const std::string& symbol = "", int limit = 100) = 0;
    virtual std::future<Order> fetch_order(const std::string& order_id, const std::string& symbol = "") = 0;
    virtual std::future<std::vector<Trade>> fetch_my_trades(const std::string& symbol = "", int limit = 100) = 0;
    
    // Trading operations
    virtual std::future<Order> create_order(const std::string& symbol, const std::string& type,
                                           const std::string& side, double amount, 
                                           double price = 0.0, const std::string& client_order_id = "") = 0;
    virtual std::future<Order> cancel_order(const std::string& order_id, const std::string& symbol = "") = 0;
    virtual std::future<std::vector<Order>> cancel_all_orders(const std::string& symbol = "") = 0;
    
    // Optional: Position management (for derivatives exchanges)
    virtual std::future<std::vector<Position>> fetch_positions(const std::string& symbol = "") {
        throw NotImplemented("fetch_positions not implemented for this exchange");
    }
    
    // Connection management
    virtual std::future<void> connect() = 0;
    virtual std::future<void> disconnect() = 0;
    virtual bool is_connected() const = 0;
    
    // Exchange information
    virtual std::string get_exchange_name() const = 0;
    virtual std::vector<std::string> get_supported_timeframes() const {
        return {"1m", "5m", "15m", "30m", "1h", "4h", "1d"};
    }
    
    // Rate limiting
    virtual int get_rate_limit() const { return 1000; } // Default 1 request per second
    
    // Validation helpers
    virtual bool has_api_credentials() const {
        return !api_key_.empty() && !secret_.empty();
    }
    
    // Static factory method for creating exchange instances
    static std::unique_ptr<Exchange> create(const std::string& exchange_name, 
                                          const nlohmann::json& config = {});
};

} // namespace ccxt
