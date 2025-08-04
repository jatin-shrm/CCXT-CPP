#include "include/deribit_improved.hpp"
#include <iostream>
#include <json.hpp>

using namespace ccxt;

int main() {
    try {
        // Configuration - load from environment or config file in production
        nlohmann::json config = {
            {"apiKey", ""},      // Set your API key
            {"secret", ""},      // Set your secret
            {"sandbox", true}    // Use testnet
        };
        
        // Create Deribit exchange instance
        DeribitImproved exchange(config);
        
        std::cout << "=== CCXT C++ Improved Example ===" << std::endl;
        std::cout << "Exchange: " << exchange.get_exchange_name() << std::endl;
        std::cout << "Rate limit: " << exchange.get_rate_limit() << " req/sec" << std::endl;
        
        // Connect to exchange
        std::cout << "\nConnecting to exchange..." << std::endl;
        exchange.connect().wait();
        std::cout << "Connected: " << (exchange.is_connected() ? "Yes" : "No") << std::endl;
        
        // Fetch markets
        std::cout << "\nFetching markets..." << std::endl;
        auto markets_future = exchange.fetch_markets();
        auto markets = markets_future.get();
        std::cout << "Found " << markets.size() << " markets" << std::endl;
        
        if (!markets.empty()) {
            const auto& market = markets[0];
            std::cout << "First market: " << market.symbol 
                      << " (ID: " << market.id << ")" << std::endl;
            std::cout << "  Type: " << market.type << std::endl;
            std::cout << "  Active: " << (market.active ? "Yes" : "No") << std::endl;
            std::cout << "  Min amount: " << market.min_amount << std::endl;
            std::cout << "  Tick size: " << market.tick_size << std::endl;
        }
        
        // Fetch balance (requires API credentials)
        if (exchange.has_api_credentials()) {
            std::cout << "\nFetching balance..." << std::endl;
            try {
                auto balance_future = exchange.fetch_balance("BTC");
                auto balance = balance_future.get();
                
                std::cout << "Balance for " << balance.currency << ":" << std::endl;
                std::cout << "  Total: " << balance.total << std::endl;
                std::cout << "  Free: " << balance.free << std::endl;
                std::cout << "  Used: " << balance.used << std::endl;
                std::cout << "  Equity: " << balance.equity << std::endl;
                std::cout << "  Unrealized P&L: " << balance.unrealized_pnl << std::endl;
                
            } catch (const CCXTException& e) {
                std::cout << "Balance fetch failed: " << e.what() << std::endl;
            }
            
            // Example order creation (commented out for safety)
            /*
            std::cout << "\nCreating test order..." << std::endl;
            try {
                auto order_future = exchange.create_order(
                    "BTC-PERPETUAL",    // symbol
                    "limit",            // type
                    "buy",              // side
                    10.0,               // amount
                    30000.0             // price
                );
                auto order = order_future.get();
                
                std::cout << "Order created:" << std::endl;
                std::cout << "  ID: " << order.id << std::endl;
                std::cout << "  Symbol: " << order.symbol << std::endl;
                std::cout << "  Type: " << (order.type == OrderType::LIMIT ? "limit" : "market") << std::endl;
                std::cout << "  Side: " << (order.side == OrderSide::BUY ? "buy" : "sell") << std::endl;
                std::cout << "  Amount: " << order.amount << std::endl;
                std::cout << "  Price: " << order.price.value_or(0.0) << std::endl;
                std::cout << "  Status: " << static_cast<int>(order.status) << std::endl;
                
            } catch (const CCXTException& e) {
                std::cout << "Order creation failed: " << e.what() << std::endl;
            }
            */
            
        } else {
            std::cout << "\nSkipping balance fetch - no API credentials provided" << std::endl;
            std::cout << "To test authenticated endpoints, set apiKey and secret in config" << std::endl;
        }
        
        // Demonstrate async operations
        std::cout << "\nDemonstrating concurrent operations..." << std::endl;
        
        // Launch multiple market fetches concurrently
        auto future1 = exchange.fetch_markets();
        auto future2 = exchange.fetch_markets();
        
        auto markets1 = future1.get();
        auto markets2 = future2.get();
        
        std::cout << "Concurrent fetch 1: " << markets1.size() << " markets" << std::endl;
        std::cout << "Concurrent fetch 2: " << markets2.size() << " markets" << std::endl;
        
        // Error handling demonstration
        std::cout << "\nDemonstrating error handling..." << std::endl;
        try {
            // This should throw NotImplemented
            auto ticker_future = exchange.fetch_ticker("BTC-PERPETUAL");
            auto ticker = ticker_future.get();
        } catch (const NotImplemented& e) {
            std::cout << "Expected error: " << e.what() << std::endl;
        } catch (const CCXTException& e) {
            std::cout << "Unexpected error: " << e.what() << std::endl;
        }
        
        // Cleanup
        std::cout << "\nDisconnecting..." << std::endl;
        exchange.disconnect().wait();
        std::cout << "Disconnected: " << (!exchange.is_connected() ? "Yes" : "No") << std::endl;
        
        std::cout << "\n=== Example completed successfully ===" << std::endl;
        
    } catch (const CCXTException& e) {
        std::cerr << "CCXT Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Standard Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}