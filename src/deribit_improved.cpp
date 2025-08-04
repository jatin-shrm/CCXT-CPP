#include "include/deribit_improved.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <boost/asio/ssl.hpp>

namespace ccxt {

DeribitImproved::DeribitImproved(const nlohmann::json& config) 
    : Exchange(
        Utils::safe_string(config, "apiKey", ""),
        Utils::safe_string(config, "secret", ""),
        Utils::safe_string(config, "password", ""),
        Utils::safe_bool(config, "sandbox", true)
    ) {
    websocket_url_ = get_websocket_url();
    
    // Initialize WebSocket client
    client_.init_asio();
    setup_websocket_handlers();
    
    // Initialize auth future
    auth_promise_ = std::promise<void>();
    auth_future_ = auth_promise_.get_future();
}

DeribitImproved::~DeribitImproved() {
    should_stop_.store(true);
    
    if (connected_.load()) {
        disconnect().wait();
    }
    
    if (websocket_thread_.joinable()) {
        websocket_thread_.join();
    }
}

std::string DeribitImproved::get_websocket_url() const {
    return is_sandbox() ? 
        "wss://test.deribit.com/ws/api/v2" : 
        "wss://www.deribit.com/ws/api/v2";
}

void DeribitImproved::setup_websocket_handlers() {
    client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
        on_open(hdl);
    });
    
    client_.set_message_handler([this](websocketpp::connection_hdl hdl, message_ptr msg) {
        on_message(hdl, msg);
    });
    
    client_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
        on_fail(hdl);
    });
    
    client_.set_close_handler([this](websocketpp::connection_hdl hdl) {
        on_close(hdl);
    });
    
    client_.set_tls_init_handler([](websocketpp::connection_hdl) {
        return websocketpp::lib::make_shared<boost::asio::ssl::context>(
            boost::asio::ssl::context::tlsv12_client);
    });
}

std::future<void> DeribitImproved::connect() {
    if (connected_.load()) {
        std::promise<void> promise;
        promise.set_value();
        return promise.get_future();
    }
    
    return std::async(std::launch::async, [this]() {
        websocketpp::lib::error_code ec;
        auto connection = client_.get_connection(websocket_url_, ec);
        
        if (ec) {
            throw ConnectionError("Failed to create connection: " + ec.message());
        }
        
        client_.connect(connection);
        
        // Start WebSocket thread
        websocket_thread_ = std::thread([this]() {
            websocket_thread_function();
        });
        
        // Wait for connection to establish
        std::unique_lock<std::mutex> lock(connection_mutex_);
        connection_cv_.wait(lock, [this]() {
            return connected_.load() || should_stop_.load();
        });
        
        if (should_stop_.load()) {
            throw ConnectionError("Connection cancelled");
        }
        
        if (!connected_.load()) {
            throw ConnectionError("Failed to establish connection");
        }
    });
}

std::future<void> DeribitImproved::disconnect() {
    return std::async(std::launch::async, [this]() {
        should_stop_.store(true);
        
        if (connected_.load()) {
            websocketpp::lib::error_code ec;
            client_.close(connection_hdl_, websocketpp::close::status::normal, "Disconnect", ec);
            
            if (ec) {
                std::cerr << "Error closing connection: " << ec.message() << std::endl;
            }
        }
        
        // Wait for thread to finish
        if (websocket_thread_.joinable()) {
            websocket_thread_.join();
        }
        
        connected_.store(false);
        authenticated_.store(false);
    });
}

bool DeribitImproved::is_connected() const {
    return connected_.load();
}

void DeribitImproved::websocket_thread_function() {
    try {
        client_.run();
    } catch (const std::exception& e) {
        std::cerr << "WebSocket thread error: " << e.what() << std::endl;
        connected_.store(false);
        authenticated_.store(false);
    }
}

void DeribitImproved::on_open(websocketpp::connection_hdl hdl) {
    connection_hdl_ = hdl;
    connected_.store(true);
    connection_cv_.notify_all();
    
    std::cout << "WebSocket connection established" << std::endl;
}

void DeribitImproved::on_message(websocketpp::connection_hdl, message_ptr msg) {
    try {
        nlohmann::json response = nlohmann::json::parse(msg->get_payload());
        
        if (response.contains("id")) {
            // This is a response to a request
            handle_response(response);
        } else {
            // This is a notification/subscription update
            handle_notification(response);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing WebSocket message: " << e.what() << std::endl;
        std::cerr << "Raw message: " << msg->get_payload() << std::endl;
    }
}

void DeribitImproved::on_fail(websocketpp::connection_hdl) {
    std::cerr << "WebSocket connection failed" << std::endl;
    connected_.store(false);
    authenticated_.store(false);
    connection_cv_.notify_all();
}

void DeribitImproved::on_close(websocketpp::connection_hdl) {
    std::cout << "WebSocket connection closed" << std::endl;
    connected_.store(false);
    authenticated_.store(false);
}

void DeribitImproved::handle_response(const nlohmann::json& response) {
    if (!response.contains("id")) return;
    
    int request_id = response["id"];
    
    std::lock_guard<std::mutex> lock(pending_requests_mutex_);
    auto it = pending_requests_.find(request_id);
    if (it != pending_requests_.end()) {
        if (response.contains("error")) {
            auto exception = create_exception_from_error(response["error"]);
            it->second.set_exception(std::make_exception_ptr(exception));
        } else {
            it->second.set_value(response);
        }
        pending_requests_.erase(it);
    }
}

void DeribitImproved::handle_notification(const nlohmann::json& notification) {
    // Handle WebSocket notifications (subscriptions, etc.)
    // This would be used for real-time market data
    std::cout << "Received notification: " << notification.dump() << std::endl;
}

CCXTException DeribitImproved::create_exception_from_error(const nlohmann::json& error) {
    int code = Utils::safe_integer(error, "code", 0);
    std::string message = Utils::safe_string(error, "message", "Unknown error");
    
    // Map Deribit error codes to appropriate exceptions
    switch (code) {
        case 13004: return AuthenticationError(message);
        case 13003: return InvalidCredentials(message);
        case 13012: return PermissionDenied(message);
        case 10028: return RateLimitExceeded(message);
        case 11029: return InsufficientFunds(message);
        case 10004: return InvalidOrder(message);
        case 11036: return OrderNotFound(message);
        case 11037: return OrderNotCancelable(message);
        case 10009: return InvalidSymbol(message);
        default: return ExchangeError(message + " (code: " + std::to_string(code) + ")");
    }
}

void DeribitImproved::check_rate_limit() {
    std::lock_guard<std::mutex> lock(rate_limit_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    auto one_second_ago = now - std::chrono::seconds(1);
    
    // Remove timestamps older than 1 second
    while (!request_timestamps_.empty() && request_timestamps_.front() < one_second_ago) {
        request_timestamps_.pop();
    }
    
    // Check if we've hit the rate limit
    if (request_timestamps_.size() >= static_cast<size_t>(get_rate_limit())) {
        auto sleep_time = request_timestamps_.front() + std::chrono::seconds(1) - now;
        if (sleep_time > std::chrono::milliseconds(0)) {
            std::this_thread::sleep_for(sleep_time);
        }
    }
    
    request_timestamps_.push(now);
}

std::future<nlohmann::json> DeribitImproved::send_request(const std::string& method, 
                                                        const nlohmann::json& params,
                                                        bool requires_auth) {
    // Ensure connection
    if (!connected_.load()) {
        connect().wait();
    }
    
    // Handle authentication if required
    if (requires_auth && !authenticated_.load()) {
        authenticate_internal().wait();
    }
    
    // Apply rate limiting
    check_rate_limit();
    
    // Generate request ID
    int request_id = next_request_id_.fetch_add(1);
    
    // Build request
    nlohmann::json request = {
        {"jsonrpc", "2.0"},
        {"id", request_id},
        {"method", method}
    };
    
    if (!params.empty()) {
        request["params"] = params;
    }
    
    // Create promise for response
    std::promise<nlohmann::json> promise;
    auto future = promise.get_future();
    
    // Store pending request
    {
        std::lock_guard<std::mutex> lock(pending_requests_mutex_);
        pending_requests_.emplace(request_id, std::move(promise));
    }
    
    // Send request
    websocketpp::lib::error_code ec;
    client_.send(connection_hdl_, request.dump(), websocketpp::frame::opcode::text, ec);
    
    if (ec) {
        // Remove from pending requests
        {
            std::lock_guard<std::mutex> lock(pending_requests_mutex_);
            pending_requests_.erase(request_id);
        }
        throw NetworkError("Failed to send request: " + ec.message());
    }
    
    return future;
}

std::future<void> DeribitImproved::authenticate_internal() {
    if (!has_api_credentials()) {
        throw InvalidCredentials("API credentials not provided");
    }
    
    std::lock_guard<std::mutex> lock(auth_mutex_);
    
    // Check if already authenticated and token not expired
    auto now = std::chrono::system_clock::now();
    if (authenticated_.load() && now < token_expiry_) {
        std::promise<void> promise;
        promise.set_value();
        return promise.get_future();
    }
    
    nlohmann::json params = {
        {"grant_type", "client_credentials"},
        {"client_id", get_api_key()},
        {"client_secret", get_secret()}
    };
    
    return std::async(std::launch::async, [this, params]() {
        auto response = send_request("public/auth", params, false).get();
        
        if (response.contains("result")) {
            auto result = response["result"];
            access_token_ = Utils::safe_string(result, "access_token");
            
            int expires_in = Utils::safe_integer(result, "expires_in", 3600);
            token_expiry_ = std::chrono::system_clock::now() + 
                          std::chrono::seconds(expires_in - 300); // 5 min buffer
            
            authenticated_.store(true);
            std::cout << "Successfully authenticated with Deribit" << std::endl;
        } else {
            throw AuthenticationError("Authentication response missing result");
        }
    });
}

// Market data methods implementation
std::future<std::vector<Market>> DeribitImproved::fetch_markets() {
    return std::async(std::launch::async, [this]() {
        // Check cache first
        {
            std::lock_guard<std::mutex> lock(markets_mutex_);
            auto now = std::chrono::system_clock::now();
            auto cache_age = std::chrono::duration_cast<std::chrono::seconds>(
                now - markets_cache_time_).count();
            
            if (!cached_markets_.empty() && cache_age < MARKETS_CACHE_TTL_SECONDS) {
                return cached_markets_;
            }
        }
        
        // Fetch from exchange
        nlohmann::json params = {
            {"currency", "BTC"},
            {"kind", "future"}
        };
        
        auto response = send_request("public/get_instruments", params, false).get();
        
        std::vector<Market> markets;
        if (response.contains("result")) {
            for (const auto& instrument : response["result"]) {
                markets.push_back(parse_market(instrument));
            }
        }
        
        // Update cache
        {
            std::lock_guard<std::mutex> lock(markets_mutex_);
            cached_markets_ = markets;
            markets_cache_time_ = std::chrono::system_clock::now();
        }
        
        return markets;
    });
}

std::future<Balance> DeribitImproved::fetch_balance(const std::string& currency) {
    return std::async(std::launch::async, [this, currency]() {
        std::string curr = currency.empty() ? "BTC" : currency;
        
        nlohmann::json params = {{"currency", curr}};
        auto response = send_request("private/get_account_summary", params, true).get();
        
        if (response.contains("result")) {
            return parse_balance(response["result"]);
        }
        
        throw ParseError("Invalid balance response");
    });
}

// Parser implementations
Balance DeribitImproved::parse_balance(const nlohmann::json& data) {
    Balance balance;
    balance.currency = safe_string(data, "currency", "BTC");
    balance.equity = safe_float(data, "equity", 0.0);
    balance.free = safe_float(data, "available_funds", 0.0);
    balance.used = balance.equity - balance.free;
    balance.total = balance.equity;
    balance.maintenance_margin = safe_float(data, "maintenance_margin", 0.0);
    balance.initial_margin = safe_float(data, "initial_margin", 0.0);
    balance.unrealized_pnl = safe_float(data, "total_pl", 0.0);
    balance.info = data;
    
    return balance;
}

Market DeribitImproved::parse_market(const nlohmann::json& data) {
    Market market;
    market.id = safe_string(data, "instrument_name");
    market.symbol = normalize_symbol(market.id);
    market.base = safe_string(data, "base_currency", "BTC");
    market.quote = safe_string(data, "quote_currency", "USD");
    market.type = safe_string(data, "kind", "future");
    market.active = safe_bool(data, "is_active", false);
    market.min_amount = safe_float(data, "min_trade_amount", 0.0);
    market.tick_size = safe_float(data, "tick_size", 0.0);
    market.contract_size = safe_float(data, "contract_size", 1.0);
    
    int64_t expiry_timestamp = safe_timestamp(data, "expiration_timestamp", 0);
    if (expiry_timestamp > 0) {
        market.expiry = Utils::timestamp_to_timepoint(expiry_timestamp);
    }
    
    market.info = data;
    return market;
}

std::string DeribitImproved::normalize_symbol(const std::string& symbol) {
    // Convert Deribit symbols to unified format
    // Example: "BTC-PERPETUAL" -> "BTC/USD:USD"
    if (symbol.find("PERPETUAL") != std::string::npos) {
        std::string base = symbol.substr(0, symbol.find('-'));
        return base + "/USD:USD";
    }
    return symbol;
}

// Implement remaining methods...
std::future<Order> DeribitImproved::create_order(const std::string& symbol, const std::string& type,
                                               const std::string& side, double amount, 
                                               double price, const std::string& client_order_id) {
    return std::async(std::launch::async, [=]() {
        // Validate parameters
        Utils::validate_positive_number(amount, "amount");
        if (type == "limit") {
            Utils::validate_positive_number(price, "price");
        }
        
        std::string method = (Utils::to_lower(side) == "buy") ? "private/buy" : "private/sell";
        
        nlohmann::json params = {
            {"instrument_name", symbol},
            {"amount", amount},
            {"type", Utils::to_lower(type)}
        };
        
        if (type == "limit" && price > 0) {
            params["price"] = price;
        }
        
        if (!client_order_id.empty()) {
            params["label"] = client_order_id;
        }
        
        auto response = send_request(method, params, true).get();
        
        if (response.contains("result") && response["result"].contains("order")) {
            return parse_order(response["result"]["order"]);
        }
        
        throw ParseError("Invalid create order response");
    });
}

Order DeribitImproved::parse_order(const nlohmann::json& data) {
    Order order;
    order.id = safe_string(data, "order_id");
    order.client_order_id = safe_string(data, "label");
    order.symbol = safe_string(data, "instrument_name");
    order.type = string_to_order_type(safe_string(data, "order_type", "limit"));
    order.side = string_to_order_side(safe_string(data, "direction", "buy"));
    order.amount = safe_float(data, "amount", 0.0);
    order.filled = safe_float(data, "filled_amount", 0.0);
    order.remaining = order.amount - order.filled;
    order.price = safe_float(data, "price", 0.0);
    order.average_price = safe_float(data, "average_price", 0.0);
    order.status = string_to_order_status(safe_string(data, "order_state", "open"));
    order.timestamp = Utils::timestamp_to_timepoint(safe_timestamp(data, "creation_timestamp", 0));
    order.info = data;
    
    return order;
}

OrderType DeribitImproved::string_to_order_type(const std::string& type_str) {
    std::string lower_type = Utils::to_lower(type_str);
    if (lower_type == "market") return OrderType::MARKET;
    if (lower_type == "limit") return OrderType::LIMIT;
    if (lower_type == "stop") return OrderType::STOP;
    if (lower_type == "stop_limit") return OrderType::STOP_LIMIT;
    return OrderType::LIMIT; // Default
}

OrderSide DeribitImproved::string_to_order_side(const std::string& side_str) {
    return Utils::to_lower(side_str) == "buy" ? OrderSide::BUY : OrderSide::SELL;
}

OrderStatus DeribitImproved::string_to_order_status(const std::string& status_str) {
    std::string lower_status = Utils::to_lower(status_str);
    if (lower_status == "open") return OrderStatus::OPEN;
    if (lower_status == "filled") return OrderStatus::FILLED;
    if (lower_status == "cancelled") return OrderStatus::CANCELED;
    if (lower_status == "rejected") return OrderStatus::REJECTED;
    return OrderStatus::OPEN; // Default
}

// Stub implementations for remaining methods
std::future<Ticker> DeribitImproved::fetch_ticker(const std::string& symbol) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("fetch_ticker not yet implemented");
        return Ticker{};
    });
}

std::future<OrderBook> DeribitImproved::fetch_order_book(const std::string& symbol, int limit) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("fetch_order_book not yet implemented");
        return OrderBook{};
    });
}

std::future<std::vector<Order>> DeribitImproved::fetch_orders(const std::string& symbol, int limit) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("fetch_orders not yet implemented");
        return std::vector<Order>{};
    });
}

std::future<std::vector<Order>> DeribitImproved::fetch_open_orders(const std::string& symbol, int limit) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("fetch_open_orders not yet implemented");
        return std::vector<Order>{};
    });
}

std::future<std::vector<Order>> DeribitImproved::fetch_closed_orders(const std::string& symbol, int limit) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("fetch_closed_orders not yet implemented");
        return std::vector<Order>{};
    });
}

std::future<Order> DeribitImproved::fetch_order(const std::string& order_id, const std::string& symbol) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("fetch_order not yet implemented");
        return Order{};
    });
}

std::future<std::vector<Trade>> DeribitImproved::fetch_my_trades(const std::string& symbol, int limit) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("fetch_my_trades not yet implemented");
        return std::vector<Trade>{};
    });
}

std::future<Order> DeribitImproved::cancel_order(const std::string& order_id, const std::string& symbol) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("cancel_order not yet implemented");
        return Order{};
    });
}

std::future<std::vector<Order>> DeribitImproved::cancel_all_orders(const std::string& symbol) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("cancel_all_orders not yet implemented");
        return std::vector<Order>{};
    });
}

std::future<std::vector<Position>> DeribitImproved::fetch_positions(const std::string& symbol) {
    return std::async(std::launch::async, [=]() {
        throw NotImplemented("fetch_positions not yet implemented");
        return std::vector<Position>{};
    });
}

Trade DeribitImproved::parse_trade(const nlohmann::json& data) {
    Trade trade;
    // Implementation would go here
    return trade;
}

Ticker DeribitImproved::parse_ticker(const nlohmann::json& data) {
    Ticker ticker;
    // Implementation would go here
    return ticker;
}

OrderBook DeribitImproved::parse_order_book(const nlohmann::json& data) {
    OrderBook orderbook;
    // Implementation would go here
    return orderbook;
}

Position DeribitImproved::parse_position(const nlohmann::json& data) {
    Position position;
    // Implementation would go here
    return position;
}

} // namespace ccxt