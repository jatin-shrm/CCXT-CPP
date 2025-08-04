#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <optional>
#include <json.hpp>

namespace ccxt {

enum class OrderType { 
    MARKET, 
    LIMIT, 
    STOP, 
    STOP_LIMIT 
};

enum class OrderSide { 
    BUY, 
    SELL 
};

enum class OrderStatus { 
    OPEN, 
    CLOSED, 
    CANCELED, 
    EXPIRED, 
    REJECTED, 
    FILLED,
    PARTIALLY_FILLED
};

struct Market {
    std::string id;           // Exchange-specific ID (e.g., "BTC-PERPETUAL")
    std::string symbol;       // Unified symbol (e.g., "BTC/USDT")
    std::string base;         // Base currency (e.g., "BTC")
    std::string quote;        // Quote currency (e.g., "USDT")
    std::string type;         // "future", "option", "spot"
    bool active;              // Whether trading is active
    double min_amount;        // Minimum order size
    double max_amount;        // Maximum order size
    double tick_size;         // Minimum price increment
    double contract_size;     // Size of one contract (for futures)
    std::optional<std::chrono::system_clock::time_point> expiry;  // For futures/options
    nlohmann::json info;      // Raw exchange data
    
    Market() : active(false), min_amount(0.0), max_amount(0.0), 
               tick_size(0.0), contract_size(1.0) {}
};

struct Balance {
    std::string currency;           // Currency code (e.g., "BTC")
    double total;                   // Total balance
    double free;                    // Available for trading
    double used;                    // Currently in orders
    double equity;                  // For futures - account equity
    double maintenance_margin;      // Required maintenance margin
    double initial_margin;          // Required initial margin
    double unrealized_pnl;          // Unrealized P&L
    nlohmann::json info;            // Raw exchange data
    
    Balance() : total(0.0), free(0.0), used(0.0), equity(0.0),
                maintenance_margin(0.0), initial_margin(0.0), unrealized_pnl(0.0) {}
};

struct Order {
    std::string id;                 // Exchange order ID
    std::string client_order_id;    // Client-specified ID
    std::string symbol;             // Trading pair symbol
    OrderType type;                 // Order type
    OrderSide side;                 // Buy or sell
    double amount;                  // Total order amount
    double filled;                  // Amount already filled
    double remaining;               // Amount remaining to fill
    std::optional<double> price;    // Limit price (if applicable)
    std::optional<double> stop_price;    // Stop price (if applicable)
    std::optional<double> average_price; // Average fill price
    OrderStatus status;             // Current order status
    std::chrono::system_clock::time_point timestamp;  // Order creation time
    std::optional<std::chrono::system_clock::time_point> last_trade_timestamp;
    std::vector<std::string> trades;     // Associated trade IDs
    double fee_cost;                // Fee paid
    std::string fee_currency;       // Fee currency
    nlohmann::json info;            // Raw exchange data
    
    Order() : type(OrderType::LIMIT), side(OrderSide::BUY), amount(0.0), 
              filled(0.0), remaining(0.0), status(OrderStatus::OPEN),
              fee_cost(0.0) {}
};

struct Trade {
    std::string id;                 // Trade ID
    std::string order;              // Associated order ID
    std::string symbol;             // Trading pair
    OrderSide side;                 // Buy or sell
    double amount;                  // Trade amount
    double price;                   // Trade price
    double cost;                    // Total cost (amount * price)
    double fee_cost;                // Fee paid
    std::string fee_currency;       // Fee currency
    std::chrono::system_clock::time_point timestamp;  // Trade time
    nlohmann::json info;            // Raw exchange data
    
    Trade() : side(OrderSide::BUY), amount(0.0), price(0.0), 
              cost(0.0), fee_cost(0.0) {}
};

struct Ticker {
    std::string symbol;             // Trading pair symbol
    double bid;                     // Best bid price
    double ask;                     // Best ask price
    double last;                    // Last trade price
    double volume;                  // 24h volume in base currency
    double quote_volume;            // 24h volume in quote currency
    double high;                    // 24h high price
    double low;                     // 24h low price
    double open;                    // 24h opening price
    double close;                   // 24h closing price (same as last)
    double change;                  // Absolute price change
    double percentage;              // Percentage price change
    std::chrono::system_clock::time_point timestamp;  // Ticker timestamp
    nlohmann::json info;            // Raw exchange data
    
    Ticker() : bid(0.0), ask(0.0), last(0.0), volume(0.0), quote_volume(0.0),
               high(0.0), low(0.0), open(0.0), close(0.0), change(0.0), percentage(0.0) {}
};

struct OrderBookLevel {
    double price;                   // Price level
    double size;                    // Total size at this level
    
    OrderBookLevel(double p, double s) : price(p), size(s) {}
};

struct OrderBook {
    std::string symbol;             // Trading pair symbol
    std::vector<OrderBookLevel> bids;    // Bid levels (price, size)
    std::vector<OrderBookLevel> asks;    // Ask levels (price, size)
    std::chrono::system_clock::time_point timestamp;  // Order book timestamp
    int64_t nonce;                  // Sequence number
    nlohmann::json info;            // Raw exchange data
    
    OrderBook() : nonce(0) {}
};

struct Position {
    std::string symbol;             // Trading pair symbol
    OrderSide side;                 // Long or short
    double size;                    // Position size
    double contracts;               // Number of contracts
    double contract_size;           // Size per contract
    double entry_price;             // Average entry price
    double mark_price;              // Current mark price
    double unrealized_pnl;          // Unrealized P&L
    double realized_pnl;            // Realized P&L
    double initial_margin;          // Required initial margin
    double maintenance_margin;      // Required maintenance margin
    std::chrono::system_clock::time_point timestamp;
    nlohmann::json info;            // Raw exchange data
    
    Position() : side(OrderSide::BUY), size(0.0), contracts(0.0), contract_size(1.0),
                 entry_price(0.0), mark_price(0.0), unrealized_pnl(0.0), realized_pnl(0.0),
                 initial_margin(0.0), maintenance_margin(0.0) {}
};

// Helper function to convert string to OrderType
OrderType string_to_order_type(const std::string& type_str);

// Helper function to convert OrderType to string
std::string order_type_to_string(OrderType type);

// Helper function to convert string to OrderSide
OrderSide string_to_order_side(const std::string& side_str);

// Helper function to convert OrderSide to string
std::string order_side_to_string(OrderSide side);

// Helper function to convert string to OrderStatus
OrderStatus string_to_order_status(const std::string& status_str);

// Helper function to convert OrderStatus to string
std::string order_status_to_string(OrderStatus status);

} // namespace ccxt