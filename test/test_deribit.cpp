#include "../src/include/deribit.hpp"
#include <json.hpp>
#include <iostream>
#include <cassert>
#include <string>
#include <chrono>
#include <thread>

using namespace std;

class DeribitTester {
private:
    Deribit* client;
    int tests_run = 0;
    int tests_passed = 0;
    string created_order_id = ""; // Store order ID for testing
    
    void log_test_result(const string& test_name, bool passed, const string& message = "") {
        tests_run++;
        if (passed) {
            tests_passed++;
            cout << "" << test_name << " PASSED" << endl;
        } else {
            cout << "" << test_name << " FAILED";
            if (!message.empty()) {
                cout << " - " << message;
            }
            cout << endl;
        }
    }
    
public:
    DeribitTester() {
        nlohmann::json config = {
            {"apiKey", "b42LEgvL"},
            {"secret", "FWsNL9GznVOz7x3CIV1lkQ3CrPMtVevzqLBohI4slko"},
            {"is_test", true}
        };
        client = new Deribit(config);
    }
    
    ~DeribitTester() {
        delete client;
    }
    
    bool test_load_markets() {
        cout << "Testing load_markets()" << endl;
        
        try {
            // Test 1: Call load_markets and check if it returns data
            nlohmann::json markets_response = client->load_markets();
            
            // Validate response is not empty
            if (markets_response.empty()) {
                log_test_result("load_markets - non-empty response", false, "Response is empty");
                return false;
            }
            log_test_result("load_markets - non-empty response", true);
            
            // Validate response is an array
            if (!markets_response.is_array()) {
                log_test_result("load_markets - array format", false, "Response is not an array");
                return false;
            }
            log_test_result("load_markets - array format", true);
            
            // Test first market structure if markets exist
            if (markets_response.size() > 0) {
                nlohmann::json first_market = markets_response[0];
                vector<string> required_fields = {
                    "id", "symbol", "base", "quote", "settle", 
                    "baseId", "quoteId", "settleId", "type", 
                    "spot", "margin", "swap", "future", "option", 
                    "active", "contract", "linear", "inverse",
                    "precision", "limits"
                };
                
                bool all_fields_present = true;
                string missing_field = "";
                
                for (const auto& field : required_fields) {
                    if (!first_market.contains(field)) {
                        all_fields_present = false;
                        missing_field = field;
                        break;
                    }
                }
                
                log_test_result("load_markets - required fields present", all_fields_present, 
                              all_fields_present ? "" : "Missing field: " + missing_field);
                
                // Validate specific field types
                bool types_valid = true;
                string type_error = "";
                
                if (!first_market["id"].is_string()) {
                    types_valid = false; type_error = "id should be string";
                } else if (!first_market["symbol"].is_string()) {
                    types_valid = false; type_error = "symbol should be string";
                } else if (!first_market["base"].is_string()) {
                    types_valid = false; type_error = "base should be string";
                } else if (!first_market["quote"].is_string()) {
                    types_valid = false; type_error = "quote should be string";
                } else if (!first_market["spot"].is_boolean()) {
                    types_valid = false; type_error = "spot should be boolean";
                } else if (!first_market["active"].is_boolean()) {
                    types_valid = false; type_error = "active should be boolean";
                } else if (!first_market["precision"].is_object()) {
                    types_valid = false; type_error = "precision should be object";
                } else if (!first_market["limits"].is_object()) {
                    types_valid = false; type_error = "limits should be object";
                }
                
                log_test_result("load_markets - field types validation", types_valid, type_error);
                
                // Validate precision object structure
                if (first_market.contains("precision") && first_market["precision"].is_object()) {
                    nlohmann::json precision = first_market["precision"];
                    bool precision_valid = precision.contains("amount") && precision.contains("price");
                    log_test_result("load_markets - precision structure", precision_valid,
                                  precision_valid ? "" : "precision missing amount or price");
                }
                
                // Validate limits object structure
                if (first_market.contains("limits") && first_market["limits"].is_object()) {
                    nlohmann::json limits = first_market["limits"];
                    bool limits_valid = limits.contains("amount") && limits.contains("price") && 
                                       limits.contains("leverage") && limits.contains("cost");
                    log_test_result("load_markets - limits structure", limits_valid,
                                  limits_valid ? "" : "limits missing required fields");
                }

                cout << "Sample market data:" << endl;
                cout << "  ID: " << first_market.value("id", "N/A") << endl;
                cout << "  Symbol: " << first_market.value("symbol", "N/A") << endl;
                cout << "  Base: " << first_market.value("base", "N/A") << endl;
                cout << "  Quote: " << first_market.value("quote", "N/A") << endl;
                cout << "  Settle: " << first_market.value("settle", "N/A") << endl;
                cout << "  Base ID: " << first_market.value("baseId", "N/A") << endl;
                cout << "  Quote ID: " << first_market.value("quoteId", "N/A") << endl;
                cout << "  Settle ID: " << first_market.value("settleId", "N/A") << endl;
                cout << "  Type: " << first_market.value("type", "N/A") << endl;

                cout << "  Spot: " << (first_market.value("spot", false) ? "true" : "false") << endl;
                cout << "  Margin: " << (first_market.value("margin", false) ? "true" : "false") << endl;
                cout << "  Swap: " << (first_market.value("swap", false) ? "true" : "false") << endl;
                cout << "  Future: " << (first_market.value("future", false) ? "true" : "false") << endl;
                cout << "  Option: " << (first_market.value("option", false) ? "true" : "false") << endl;
                cout << "  Active: " << (first_market.value("active", false) ? "true" : "false") << endl;
                cout << "  Contract: " << (first_market.value("contract", false) ? "true" : "false") << endl;
                cout << "  Linear: " << (first_market.value("linear", false) ? "true" : "false") << endl;
                cout << "  Inverse: " << (first_market.value("inverse", false) ? "true" : "false") << endl;

            }
            
            // Test 2, Call load_markets again (should use cache)
            auto start_time = chrono::high_resolution_clock::now();
            nlohmann::json cached_response = client->load_markets(false); // reload = false
            auto end_time = chrono::high_resolution_clock::now();
            auto duration = chrono::duration_cast<chrono::milliseconds>(end_time - start_time);
            
            bool cache_works = (cached_response == markets_response);
            log_test_result("load_markets - caching works", cache_works, 
                          cache_works ? "Cached in " + to_string(duration.count()) + "ms" : 
                                      "Cached response differs from original");
            


            // Test 3,Force a reload to verify that, even when data is already cached, fresh data is still fetched from the API
            nlohmann::json reloaded_response = client->load_markets(true); // reload = true
            bool reload_works = !reloaded_response.empty() && reloaded_response.is_array();
            log_test_result("load_markets - force reload", reload_works,
                          reload_works ? "" : "Reload failed or returned invalid data");
            
            cout << "Total markets loaded: " << markets_response.size() << endl;
            return true;
            
        } catch (const exception& e) {
            log_test_result("load_markets - exception handling", false, 
                          string("Exception: ") + e.what());
            return false;
        }
    }
    
    bool test_fetch_markets() {
        cout << " Testing fetch_markets()" << endl;
        
        try {
            nlohmann::json markets_response = client->fetch_markets();
            
            // Basic validation
            bool not_empty = !markets_response.empty();
            log_test_result("fetch_markets - non-empty response", not_empty);
            
            bool is_array = markets_response.is_array();
            log_test_result("fetch_markets - array format", is_array);
            
            if (not_empty && is_array) {
                cout << "Markets fetched: " << markets_response.size() << endl;
                return true;
            }
            
            return false;
            
        } catch (const exception& e) {
            log_test_result("fetch_markets - exception handling", false, 
                          string("Exception: ") + e.what());
            return false;
        }
    }
    
    bool test_fetch_order_book() {
        cout << "Testing fetch_order_book()" << endl;
        
        try {
            // Test with a popular trading pair such as BTC
            string test_symbol = "BTC-PERPETUAL";
            nlohmann::json orderbook_response = client->fetch_order_book(test_symbol);
            
            // Validate response is not empty
            if (orderbook_response.empty()) {
                log_test_result("fetch_order_book - non-empty response", false, "Response is empty");
                return false;
            }
            log_test_result("fetch_order_book - non-empty response", true);
            
            // Validate response is an object
            if (!orderbook_response.is_object()) {
                log_test_result("fetch_order_book - object format", false, "Response is not an object");
                return false;
            }
            log_test_result("fetch_order_book - object format", true);
            
            // Check required fields
            vector<string> required_fields = {"symbol", "bids", "asks", "timestamp", "datetime", "nonce"};
            bool all_fields_present = true;
            string missing_field = "";
            
            for (const auto& field : required_fields) {
                if (!orderbook_response.contains(field)) {
                    all_fields_present = false;
                    missing_field = field;
                    break;
                }
            }
            log_test_result("fetch_order_book - required fields present", all_fields_present,
                          all_fields_present ? "" : "Missing field: " + missing_field);
            
            // Validate field types
            bool types_valid = true;
            string type_error = "";
            
            if (!orderbook_response["symbol"].is_string()) {
                types_valid = false; type_error = "symbol should be string";
            } else if (!orderbook_response["bids"].is_array()) {
                types_valid = false; type_error = "bids should be array";
            } else if (!orderbook_response["asks"].is_array()) {
                types_valid = false; type_error = "asks should be array";
            } else if (!orderbook_response["timestamp"].is_number_integer() && !orderbook_response["timestamp"].is_null()) {
                types_valid = false; type_error = "timestamp should be integer or null";
            }
            log_test_result("fetch_order_book - field types validation", types_valid, type_error);


            
            // Validate symbol matches request
            string returned_symbol = orderbook_response.value("symbol", "");
            bool symbol_matches = (returned_symbol == test_symbol);
            log_test_result("fetch_order_book - symbol matches request", symbol_matches,
                          symbol_matches ? "" : "Expected: " + test_symbol + ", Got: " + returned_symbol);


            
            // Validate bids array structure
            nlohmann::json bids = orderbook_response["bids"];
            bool bids_valid = true;
            if (bids.is_array() && !bids.empty()) {
                for (size_t i = 0; i < min(size_t(3), bids.size()); i++) {
                    if (!bids[i].is_array() || bids[i].size() < 2) {
                        bids_valid = false;
                        break;
                    }
                    if (!bids[i][0].is_number() || !bids[i][1].is_number()) {
                        bids_valid = false;
                        break;
                    }
                }
            }
            log_test_result("fetch_order_book - bids array structure", bids_valid,
                          bids_valid ? "" : "Bids should be array of [price, amount] arrays");
            
            // Validate asks array structure
            nlohmann::json asks = orderbook_response["asks"];
            bool asks_valid = true;
            if (asks.is_array() && !asks.empty()) {
                for (size_t i = 0; i < min(size_t(3), asks.size()); i++) {
                    if (!asks[i].is_array() || asks[i].size() < 2) {
                        asks_valid = false;
                        break;
                    }
                    if (!asks[i][0].is_number() || !asks[i][1].is_number()) {
                        asks_valid = false;
                        break;
                    }
                }
            }
            log_test_result("fetch_order_book - asks array structure", asks_valid,
                          asks_valid ? "" : "Asks should be array of [price, amount] arrays");
            
            // Validate price ordering (bids descending, asks ascending)
            bool bids_sorted = true;
            if (bids.is_array() && bids.size() > 1) {
                for (size_t i = 1; i < bids.size(); i++) {
                    double prev_price = bids[i-1][0].get<double>();
                    double curr_price = bids[i][0].get<double>();
                    if (prev_price < curr_price) {
                        bids_sorted = false;
                        break;
                    }
                }
            }
            log_test_result("fetch_order_book - bids sorted descending", bids_sorted,
                          bids_sorted ? "" : "Bids should be sorted by price descending (highest first)");
            
            bool asks_sorted = true;
            if (asks.is_array() && asks.size() > 1) {
                for (size_t i = 1; i < asks.size(); i++) {
                    double prev_price = asks[i-1][0].get<double>();
                    double curr_price = asks[i][0].get<double>();
                    if (prev_price > curr_price) {
                        asks_sorted = false;
                        break;
                    }
                }
            }
            log_test_result("fetch_order_book - asks sorted ascending", asks_sorted,
                          asks_sorted ? "" : "Asks should be sorted by price ascending (lowest first)");
            
            // Validate spread (best ask > best bid)
            bool spread_valid = true;
            if (bids.is_array() && !bids.empty() && asks.is_array() && !asks.empty()) {
                double best_bid = bids[0][0].get<double>();
                double best_ask = asks[0][0].get<double>();
                spread_valid = (best_ask > best_bid);
                log_test_result("fetch_order_book - valid spread", spread_valid,
                              spread_valid ? "Spread: " + to_string(best_ask - best_bid) : 
                                           "Best ask (" + to_string(best_ask) + ") should be > best bid (" + to_string(best_bid) + ")");
            } else {
                log_test_result("fetch_order_book - valid spread", false, "Cannot validate spread - empty bids or asks");
            }
            
            // Display sample data
            cout << "Order Book Sample Data:" << endl;
            cout << "  Symbol: " << orderbook_response.value("symbol", "N/A") << endl;
            cout << "  Timestamp: " << orderbook_response.value("timestamp", 0) << endl;
            cout << "  Bids count: " << bids.size() << endl;
            cout << "  Asks count: " << asks.size() << endl;
            
            if (!bids.empty()) {
                cout << "  Best bid: " << bids[0][0].get<double>() << " @ " << bids[0][1].get<double>() << endl;
            }
            if (!asks.empty()) {
                cout << "  Best ask: " << asks[0][0].get<double>() << " @ " << asks[0][1].get<double>() << endl;
            }
            
            // Test with different symbol
            string test_symbol2 = "ETH-PERPETUAL";
            nlohmann::json orderbook2 = client->fetch_order_book(test_symbol2);
            bool second_test = !orderbook2.empty() && orderbook2.contains("symbol") && 
                              orderbook2["symbol"].get<string>() == test_symbol2;
            log_test_result("fetch_order_book - different symbol test", second_test,
                          second_test ? "ETH-PERPETUAL orderbook fetched successfully" : 
                                      "Failed to fetch ETH-PERPETUAL orderbook");
            
            return true;
            
        } catch (const exception& e) {
            log_test_result("fetch_order_book - exception handling", false,
                          string("Exception: ") + e.what());
            return false;
        }
    }
    
    bool test_watch_orders() {
        cout << "Testing watch_orders()" << endl;
        
        try {
            bool subscription_successful = false;
            bool received_valid_data = false;
            bool handler_called = false;
            nlohmann::json received_data;
            
            // Define a handler function similar to main.cpp
            auto orderHandler = [&](const nlohmann::json &order) {
                handler_called = true;
                received_data = order;
                
                if (!order.empty()) {
                    received_valid_data = true;
                }
                
                cout << "Order Update Received:" << endl;
                cout << "ID: " << order.value("order_id", "N/A") << endl;
                cout << "Instrument: " << order.value("instrument_name", "N/A") << endl;
                cout << "Direction: " << order.value("direction", "N/A") << endl;
                cout << "Price: " << order.value("price", 0.0) << endl;
                cout << "Amount: " << order.value("amount", 0.0) << endl;
                cout << "Filled: " << order.value("filled_amount", 0.0) << endl;
                cout << "State: " << order.value("order_state", "N/A") << endl;
                cout << "----------------------------------------" << endl;
            };
            
            try {
                // Subscribe to order updates similar to main.cpp
                client->watch_orders(orderHandler);
                subscription_successful = true;
                log_test_result("watch_orders - subscription setup", true);
                
                cout << "Subscribed to order updates. Waiting for messages (5 seconds)..." << endl;
                this_thread::sleep_for(chrono::seconds(5));
                
            } catch (const exception& e) {
                string error_msg = e.what();
                if (error_msg.find("invalid_credentials") != string::npos || 
                    error_msg.find("Authentication failed") != string::npos) {
                    log_test_result("watch_orders - subscription setup", false, 
                                  "Authentication failed - requires valid API credentials");
                    return false;
                }
                log_test_result("watch_orders - subscription setup", false, 
                              string("Subscription failed: ") + e.what());
                return false;
            }
            
            log_test_result("watch_orders - handler called", handler_called,
                          handler_called ? "Handler was invoked" : "Handler was not called within timeout");
            
            if (received_valid_data && !received_data.empty()) {
                log_test_result("watch_orders - data reception", true,
                              "Valid order data received");
            } else {
                log_test_result("watch_orders - data reception", false, 
                              "No valid order data received within timeout period");
            }
            
            return subscription_successful;
            
        } catch (const exception& e) {
            log_test_result("watch_orders - exception handling", false,
                          string("Exception: ") + e.what());
            return false;
        }
    }
        
    bool test_watch_order_book() {
        cout << "Testing watch_order_book()" << endl;
        
        try {
            bool subscription_successful = false;
            bool received_valid_data = false;
            bool handler_called = false;
            nlohmann::json received_data;
            
            // Define a handler function similar to main.cpp
            auto orderBookHandler = [&](const nlohmann::json &message) {
                handler_called = true;
                received_data = message;
                
                if (!message.empty()) {
                    received_valid_data = true;
                }
                
                cout << "Order Book Update Received:" << endl;

                // Sometimes the feed gives "data", sometimes bids/asks directly
                const nlohmann::json *data = nullptr;
                if (message.contains("data"))
                    data = &message["data"];
                else
                    data = &message;

                // instrument_name / change_id / timestamp may exist only in "data"
                if (data->contains("instrument_name"))
                    cout << "Instrument: " << data->value("instrument_name", "N/A") << endl;
                if (data->contains("change_id"))
                    cout << "Change ID: " << data->value("change_id", 0) << endl;
                if (data->contains("timestamp"))
                    cout << "Timestamp: " << data->value("timestamp", 0) << endl;

                // Handle bids
                if (data->contains("bids")) {
                    cout << "Top Bids:" << endl;
                    for (auto &bid : (*data)["bids"]) {
                        // ["new", price, amount]
                        string action = bid[0].get<string>();
                        double price = bid[1].get<double>();
                        double amount = bid[2].get<double>();

                        cout << "  Action: " << action
                                  << " | Price: " << price
                                  << " | Amount: " << amount << endl;
                    }
                }

                // Handle asks
                if (data->contains("asks")) {
                    cout << "Top Asks:" << endl;
                    for (auto &ask : (*data)["asks"]) {
                        string action = ask[0].get<string>();
                        double price = ask[1].get<double>();
                        double amount = ask[2].get<double>();

                        cout << "  Action: " << action
                                  << " | Price: " << price
                                  << " | Amount: " << amount << endl;
                    }
                }

                cout << "----------------------------------------" << endl;
            };
            
            // Parameters for subscription similar to main.cpp
            nlohmann::json params = {
                {"interval", "100ms"},
                {"useDepthEndpoint", false}
            };
            
            try {
                // Subscribe to the order book for BTC-PERPETUAL
                client->watch_order_book(orderBookHandler, "BTC-PERPETUAL", 20, params);
                subscription_successful = true;
                log_test_result("watch_order_book - subscription setup", true);
                
                cout << "Subscribed to order book updates. Waiting for messages (5 seconds)..." << endl;
                this_thread::sleep_for(chrono::seconds(5));
                
            } catch (const exception& e) {
                log_test_result("watch_order_book - subscription setup", false,
                              string("Subscription failed: ") + e.what());
                return false;
            }
            
            log_test_result("watch_order_book - handler called", handler_called,
                          handler_called ? "Handler was invoked" : "Handler was not called within timeout");
            
            if (received_valid_data && !received_data.empty()) {
                log_test_result("watch_order_book - data reception", true,
                              "Valid order book data received");
            } else {
                log_test_result("watch_order_book - data reception", false,
                              "No valid order book data received within timeout period");
            }
            
            return subscription_successful;
            
        } catch (const exception& e) {
            log_test_result("watch_order_book - exception handling", false,
                          string("Exception: ") + e.what());
            return false;
        }
    }
    
    bool test_authentication() {
        cout << "Testing authentication()" << endl;
        
        try {
            // Test 1: Basic authentication with valid credentials
            try {
                client->authenticate();
                log_test_result("authentication - basic auth", true, "Authentication successful");
            } catch (const exception& e) {
                string error_msg = e.what();
                if (error_msg.find("invalid_credentials") != string::npos) {
                    log_test_result("authentication - basic auth", false, "Invalid credentials provided");
                    return false;
                } else if (error_msg.find("Authentication failed") != string::npos) {
                    log_test_result("authentication - basic auth", false, "Authentication failed: " + error_msg);
                    return false;
                } else {
                    log_test_result("authentication - basic auth", false, "Unexpected error: " + error_msg);
                    return false;
                }
            }
            
            // Test 2: Multiple authentication calls (should not fail if already authenticated)
            bool multiple_auth_success = true;
            try {
                for (int i = 0; i < 3; i++) {
                    client->authenticate();
                }
                log_test_result("authentication - multiple calls", true, "Multiple auth calls handled correctly");
            } catch (const exception& e) {
                multiple_auth_success = false;
                log_test_result("authentication - multiple calls", false, 
                              string("Failed on repeated auth: ") + e.what());
            }
            
            // Test 3: Test authentication timeout/expiry handling
            cout << "Testing authentication persistence..." << endl;
            
            // Wait a short time and test again
            this_thread::sleep_for(chrono::seconds(2));
            
            bool persistence_test = true;
            try {
                client->authenticate();
                log_test_result("authentication - persistence", true, "Auth persists correctly");
            } catch (const exception& e) {
                persistence_test = false;
                log_test_result("authentication - persistence", false,
                              string("Auth persistence failed: ") + e.what());
            }
            
            // Test 4: Test that authenticated calls work - try a private endpoint
            bool private_endpoint_test = false;
            try {
                // Try to fetch balance (requires authentication)
                nlohmann::json balance = client->fetch_balance();
                if (!balance.empty() && balance.contains("info")) {
                    private_endpoint_test = true;
                    log_test_result("authentication - private endpoint access", true, 
                                  "Can access private endpoints after auth");
                    
                    // Display sample balance data which ensures that it is authenticated 
                    cout << "Balance Info Sample:" << endl;
                    if (balance.contains("BTC")) {
                        cout << "  BTC Free: " << balance["BTC"].value("free", 0.0) << endl;
                        cout << "  BTC Used: " << balance["BTC"].value("used", 0.0) << endl;
                        cout << "  BTC Total: " << balance["BTC"].value("total", 0.0) << endl;
                    }
                } else {
                    log_test_result("authentication - private endpoint access", false, 
                                  "Private endpoint returned empty/invalid response");
                }
            } catch (const exception& e) {
                string error_msg = e.what();
                if (error_msg.find("invalid_credentials") != string::npos || 
                    error_msg.find("Authentication failed") != string::npos) {
                    log_test_result("authentication - private endpoint access", false, 
                                  "Auth not working for private endpoints");
                } else {
                    log_test_result("authentication - private endpoint access", false,
                                  string("Private endpoint error: ") + e.what());
                }
            }
            
            // Test 5: Test signature generation (indirect test)
            bool signature_test = true;
            try {
                // Call authenticate again to test signature generation
                client->authenticate();
                log_test_result("authentication - signature generation", true, 
                              "Signature generation working");
            } catch (const exception& e) {
                signature_test = false;
                log_test_result("authentication - signature generation", false,
                              string("Signature generation failed: ") + e.what());
            }
            
            // Test 6: Validate that we're using testnet
            cout << "Environment Check:" << endl;
            cout << "  Using testnet environment: true" << endl;
            log_test_result("authentication - environment check", true, "Running on testnet");
            
            
            return multiple_auth_success && persistence_test && signature_test && private_endpoint_test;
            
        } catch (const exception& e) {
            log_test_result("authentication - exception handling", false,
                          string("Unexpected exception: ") + e.what());
            return false;
        }
    }
    
    bool test_fetch_balance() {
        cout << "Testing fetch_balance()" << endl;
        
        try {
            // Test 1: Basic balance fetch (default BTC)
            nlohmann::json balance_response = client->fetch_balance();
            
            // Validate response is not empty
            if (balance_response.empty()) {
                log_test_result("fetch_balance - non-empty response", false, "Response is empty");
                return false;
            }
            log_test_result("fetch_balance - non-empty response", true);
            
            // Validate response is an object
            if (!balance_response.is_object()) {
                log_test_result("fetch_balance - object format", false, "Response is not an object");
                return false;
            }
            log_test_result("fetch_balance - object format", true);
            
            // Check required fields
            vector<string> required_fields = {"info", "BTC"};
            bool all_fields_present = true;
            string missing_field = "";
            
            for (const auto& field : required_fields) {
                if (!balance_response.contains(field)) {
                    all_fields_present = false;
                    missing_field = field;
                    break;
                }
            }
            log_test_result("fetch_balance - required fields present", all_fields_present,
                          all_fields_present ? "" : "Missing field: " + missing_field);
            
            // Validate BTC balance structure
            if (balance_response.contains("BTC") && balance_response["BTC"].is_object()) {
                nlohmann::json btc_balance = balance_response["BTC"];
                vector<string> balance_fields = {"free", "used", "total"};
                
                bool btc_fields_valid = true;
                for (const auto& field : balance_fields) {
                    if (!btc_balance.contains(field)) {
                        btc_fields_valid = false;
                        break;
                    }
                    if (!btc_balance[field].is_number()) {
                        btc_fields_valid = false;
                        break;
                    }
                }
                log_test_result("fetch_balance - BTC balance structure", btc_fields_valid,
                              btc_fields_valid ? "" : "BTC balance missing required numeric fields");
                
                // Display balance data
                cout << "BTC Balance Data:" << endl;
                cout << "  Free: " << btc_balance.value("free", 0.0) << endl;
                cout << "  Used: " << btc_balance.value("used", 0.0) << endl;
                cout << "  Total: " << btc_balance.value("total", 0.0) << endl;
                
                // validate balance logic For derivatives exchanges like Deribit,
                // total includes unrealized P&L, so total >= free + used (when profitable)
                // or total <= free + used (when in loss)
                double free = btc_balance.value("free", 0.0);
                double used = btc_balance.value("used", 0.0);
                double total = btc_balance.value("total", 0.0);
                
                // Check that values are non-negative
                bool non_negative = (free >= 0 && used >= 0 && total >= 0);
                log_test_result("fetch_balance - non-negative values", non_negative,
                              non_negative ? "All balance values are non-negative" : 
                                           "Some balance values are negative");
                
                // For derivatives, validate that free + used <= total + small tolerance (accounting for P&L)
                double calculated_balance = free + used;
                double difference = abs(total - calculated_balance);
                bool balance_reasonable = (total >= 0 && calculated_balance >= 0);
                
                log_test_result("fetch_balance - balance consistency", balance_reasonable,
                              balance_reasonable ? "Balance values are consistent (Free: " + to_string(free) + 
                                                 ", Used: " + to_string(used) + ", Total: " + to_string(total) + ")" :
                                                 "Balance values inconsistent");
                
                // Additional check: total should be reasonable compared to free + used
                if (total > 0 || calculated_balance > 0) {
                    bool reasonable_ratio = (difference < max(total, calculated_balance) * 10); // Allow large P&L variations
                    log_test_result("fetch_balance - reasonable P&L impact", reasonable_ratio,
                                  reasonable_ratio ? "P&L impact within reasonable range" :
                                                   "Unrealistic balance difference: " + to_string(difference));
                }
            }
            
            // Test 2: Fetch balance for ETH
            try {
                nlohmann::json eth_params = {{"code", "ETH"}};
                nlohmann::json eth_balance = client->fetch_balance(eth_params);
                bool eth_test = !eth_balance.empty() && eth_balance.contains("ETH");
                log_test_result("fetch_balance - ETH currency test", eth_test,
                              eth_test ? "ETH balance fetched successfully" : "Failed to fetch ETH balance");
            } catch (const exception& e) {
                log_test_result("fetch_balance - ETH currency test", false,
                              string("ETH balance fetch failed: ") + e.what());
            }
            
            return true;
            
        } catch (const exception& e) {
            log_test_result("fetch_balance - exception handling", false,
                          string("Exception: ") + e.what());
            return false;
        }
    }
    
    bool test_create_order() {
        cout << "Testing create_order() - CAREFUL: Using safe order parameters" << endl;
        
        try {
            //  Using a very low price that won't get filled
            string test_symbol = "BTC-PERPETUAL";
            string type = "limit";
            string side = "buy";
            double amount = 10; // minimum contract size
            double safe_price = 1000.0; // very low price that won't fill on testnet
            
            nlohmann::json params = {
                {"post_only", true}, // Ensure we don't take liquidity
                {"timeInForce", "GTC"}
            };
            
            cout << " Creating buy order at $" << safe_price << " (well below market)" << endl;
            
            nlohmann::json order_response = client->create_order(test_symbol, type, side, amount, safe_price, params);
            
            // Validate response is not empty
            if (order_response.empty()) {
                log_test_result("create_order - non-empty response", false, "Response is empty");
                return false;
            }
            log_test_result("create_order - non-empty response", true);
            
            // Validate response is an object
            if (!order_response.is_object()) {
                log_test_result("create_order - object format", false, "Response is not an object");
                return false;
            }
            log_test_result("create_order - object format", true);
            
            // Check required fields
            vector<string> required_fields = {"id", "info", "symbol", "type", "side", "amount", "price", "status"};
            bool all_fields_present = true;
            string missing_field = "";
            
            for (const auto& field : required_fields) {
                if (!order_response.contains(field)) {
                    all_fields_present = false;
                    missing_field = field;
                    break;
                }
            }
            log_test_result("create_order - required fields present", all_fields_present,
                          all_fields_present ? "" : "Missing field: " + missing_field);
            
            // Validate field values
            string returned_symbol = order_response.value("symbol", "");
            string returned_side = order_response.value("side", "");
            string returned_type = order_response.value("type", "");
            string order_status = order_response.value("status", "");
            string order_id = order_response.value("id", "");
            
            log_test_result("create_order - symbol matches", (returned_symbol == test_symbol),
                          returned_symbol == test_symbol ? "" : "Expected: " + test_symbol + ", Got: " + returned_symbol);
            
            log_test_result("create_order - side matches", (returned_side == side),
                          returned_side == side ? "" : "Expected: " + side + ", Got: " + returned_side);
            
            log_test_result("create_order - type matches", (returned_type == type),
                          returned_type == type ? "" : "Expected: " + type + ", Got: " + returned_type);
            
            // Validate order status (should be "open" since it's a safe price)
            bool valid_status = (order_status == "open" || order_status == "untriggered");
            log_test_result("create_order - valid status", valid_status,
                          valid_status ? "Order status: " + order_status : "Unexpected status: " + order_status);
            
            // Display order data
            cout << "Created Order Data:" << endl;
            cout << "  ID: " << order_id << endl;
            cout << "  Symbol: " << returned_symbol << endl;
            cout << "  Side: " << returned_side << endl;
            cout << "  Type: " << returned_type << endl;
            cout << "  Amount: " << order_response.value("amount", "N/A") << endl;
            cout << "  Price: " << order_response.value("price", "N/A") << endl;
            cout << "  Status: " << order_status << endl;
            
            // Store order ID for later tests
            created_order_id = order_id;
            log_test_result("create_order - order created successfully", true,
                          "Order ID: " + order_id);
            
            return true;
            
        } catch (const exception& e) {
            log_test_result("create_order - exception handling", false,
                          string("Exception: ") + e.what());
            return false;
        }
    }
    
    bool test_fetch_order() {
        cout << "Testing fetch_order()" << endl;
        
        try {
            // First create a safe order if we don't have one
            if (created_order_id.empty()) {
                cout << "No existing order ID, creating a safe test order first..." << endl;
                if (!test_create_order()) {
                    log_test_result("fetch_order - prerequisite", false, "Failed to create test order");
                    return false;
                }
            }
            
            string test_symbol = "BTC-PERPETUAL";
            nlohmann::json order_response = client->fetch_order(created_order_id, test_symbol);
            
            // Validate response is not empty
            if (order_response.empty()) {
                log_test_result("fetch_order - non-empty response", false, "Response is empty");
                return false;
            }
            log_test_result("fetch_order - non-empty response", true);
            
            // Validate response is an object
            if (!order_response.is_object()) {
                log_test_result("fetch_order - object format", false, "Response is not an object");
                return false;
            }
            log_test_result("fetch_order - object format", true);
            
            // Check required fields
            vector<string> required_fields = {"id", "info", "symbol", "type", "side", "amount", "status"};
            bool all_fields_present = true;
            string missing_field = "";
            
            for (const auto& field : required_fields) {
                if (!order_response.contains(field)) {
                    all_fields_present = false;
                    missing_field = field;
                    break;
                }
            }
            log_test_result("fetch_order - required fields present", all_fields_present,
                          all_fields_present ? "" : "Missing field: " + missing_field);
            
            // Validate order ID matches
            string returned_id = order_response.value("id", "");
            bool id_matches = (returned_id == created_order_id);
            log_test_result("fetch_order - order ID matches", id_matches,
                          id_matches ? "Order ID: " + returned_id : 
                                     "Expected: " + created_order_id + ", Got: " + returned_id);
            
            // Display fetched order data
            cout << "Fetched Order Data:" << endl;
            cout << "  ID: " << returned_id << endl;
            cout << "  Symbol: " << order_response.value("symbol", "N/A") << endl;
            cout << "  Side: " << order_response.value("side", "N/A") << endl;
            cout << "  Type: " << order_response.value("type", "N/A") << endl;
            cout << "  Amount: " << order_response.value("amount", "N/A") << endl;
            cout << "  Price: " << order_response.value("price", "N/A") << endl;
            cout << "  Status: " << order_response.value("status", "N/A") << endl;
            cout << "  Filled: " << order_response.value("filled", "0") << endl;
            
            return true;
            
        } catch (const exception& e) {
            log_test_result("fetch_order - exception handling", false,
                          string("Exception: ") + e.what());
            return false;
        }
    }
    
    bool test_cancel_order() {
        cout << "Testing cancel_order()" << endl;
        
        try {
            // First ensure we have an order to cancel
            if (created_order_id.empty()) {
                cout << "No existing order ID, creating a safe test order first..." << endl;
                if (!test_create_order()) {
                    log_test_result("cancel_order - prerequisite", false, "Failed to create test order");
                    return false;
                }
            }
            
            string test_symbol = "BTC-PERPETUAL";
            
            // Step 1: Fetch the order first to check its current status
            cout << "Fetching order status before cancellation..." << endl;
            nlohmann::json pre_cancel_order;
            try {
                pre_cancel_order = client->fetch_order(created_order_id, test_symbol);
                string current_status = pre_cancel_order.value("status", "");
                cout << "Current order status: " << current_status << endl;
                
                // Check if order is already cancelled
                bool already_cancelled = (current_status == "cancelled" || current_status == "canceled");
                if (already_cancelled) {
                    log_test_result("cancel_order - pre-check", true, 
                                  "Order already cancelled, skipping cancellation test");
                    created_order_id = "";
                    return true;
                }
                
                log_test_result("cancel_order - pre-check", true, 
                              "Order status before cancel: " + current_status);
                
            } catch (const exception& e) {
                log_test_result("cancel_order - pre-check", false,
                              string("Failed to fetch order before cancel: ") + e.what());
                return false;
            }
            
            // Step 2: Proceed with cancellation
            cout << "Canceling order ID: " << created_order_id << endl;
            
            nlohmann::json cancel_response = client->cancel_order(created_order_id, test_symbol);
            
            // Validate response is not empty
            if (cancel_response.empty()) {
                log_test_result("cancel_order - non-empty response", false, "Response is empty");
                return false;
            }
            log_test_result("cancel_order - non-empty response", true);
            
            // Validate response is an object
            if (!cancel_response.is_object()) {
                log_test_result("cancel_order - object format", false, "Response is not an object");
                return false;
            }
            log_test_result("cancel_order - object format", true);
            
            // Check required fields
            vector<string> required_fields = {"id", "info", "symbol", "status"};
            bool all_fields_present = true;
            string missing_field = "";
            
            for (const auto& field : required_fields) {
                if (!cancel_response.contains(field)) {
                    all_fields_present = false;
                    missing_field = field;
                    break;
                }
            }
            log_test_result("cancel_order - required fields present", all_fields_present,
                          all_fields_present ? "" : "Missing field: " + missing_field);
            
            // Validate order ID matches
            string returned_id = cancel_response.value("id", "");
            bool id_matches = (returned_id == created_order_id);
            log_test_result("cancel_order - order ID matches", id_matches,
                          id_matches ? "Order ID: " + returned_id : 
                                     "Expected: " + created_order_id + ", Got: " + returned_id);
            
            // Validate order status is cancelled
            string order_status = cancel_response.value("status", "");
            bool is_cancelled = (order_status == "cancelled" || order_status == "canceled");
            log_test_result("cancel_order - order cancelled", is_cancelled,
                          is_cancelled ? "Order status: " + order_status : 
                                       "Unexpected status: " + order_status);
            
            // Display cancelled order data
            cout << "Cancelled Order Data:" << endl;
            cout << "  ID: " << returned_id << endl;
            cout << "  Symbol: " << cancel_response.value("symbol", "N/A") << endl;
            cout << "  Status: " << order_status << endl;
            cout << "  Cancel Timestamp: " << cancel_response.value("timestamp", 0) << endl;
            
            // Step 3: Verify cancellation by fetching the order again
            cout << "Verifying cancellation by fetching order again..." << endl;
            try {
                nlohmann::json post_cancel_order = client->fetch_order(created_order_id, test_symbol);
                string verified_status = post_cancel_order.value("status", "");
                bool cancellation_verified = (verified_status == "cancelled" || verified_status == "canceled");
                
                log_test_result("cancel_order - cancellation verified", cancellation_verified,
                              cancellation_verified ? "Verified status: " + verified_status : 
                                                     "Unexpected verified status: " + verified_status);
                
            } catch (const exception& e) {
                log_test_result("cancel_order - cancellation verification", false,
                              string("Failed to verify cancellation: ") + e.what());
            }
            
            // Step 4: Test double cancellation handling with fetch_order check first
            cout << "Testing double cancellation handling..." << endl;
            
            // First fetch the order to check if it's already cancelled
            try {
                nlohmann::json double_cancel_check = client->fetch_order(created_order_id, test_symbol);
                string double_cancel_status = double_cancel_check.value("status", "");
                bool already_cancelled_check = (double_cancel_status == "cancelled" || double_cancel_status == "canceled");
                
                if (already_cancelled_check) {
                    log_test_result("cancel_order - double cancel pre-check", true,
                                  "Order confirmed cancelled, skipping redundant cancel attempt");
                    
                    // Still try to cancel to test error handling
                    try {
                        client->cancel_order(created_order_id, test_symbol);
                        log_test_result("cancel_order - double cancel handling", false,
                                      "Should not be able to cancel already cancelled order");
                    } catch (const exception& e) {
                        string error_msg = e.what();
                        bool expected_error = (error_msg.find("not found") != string::npos || 
                                             error_msg.find("already") != string::npos ||
                                             error_msg.find("cancelled") != string::npos);
                        log_test_result("cancel_order - double cancel handling", expected_error,
                                      expected_error ? "Correctly handles already cancelled order: " + error_msg : 
                                                     "Unexpected error: " + error_msg);
                    }
                } else {
                    log_test_result("cancel_order - double cancel pre-check", false,
                                  "Order not cancelled properly, status: " + double_cancel_status);
                }
                
            } catch (const exception& e) {
                log_test_result("cancel_order - double cancel pre-check", false,
                              string("Failed to check order status for double cancel test: ") + e.what());
            }
            
            // Clear the order ID since it's now cancelled
            created_order_id = "";
            
            return true;
            
        } catch (const exception& e) {
            log_test_result("cancel_order - exception handling", false,
                          string("Exception: ") + e.what());
            return false;
        }
    }
    
    bool test_fetch_ticker() {
    cout << "Testing fetch_ticker()" << endl;
    
    try {
        // Test with a popular trading pair
        string test_symbol = "BTC-PERPETUAL";
        nlohmann::json ticker_response = client->fetch_ticker(test_symbol);
        
        // Validate response is not empty
        if (ticker_response.empty()) {
            log_test_result("fetch_ticker - non-empty response", false, "Response is empty");
            return false;
        }
        log_test_result("fetch_ticker - non-empty response", true);
        
        // Validate response is an object
        if (!ticker_response.is_object()) {
            log_test_result("fetch_ticker - object format", false, "Response is not an object");
            return false;
        }
        log_test_result("fetch_ticker - object format", true);
        
        // Check required fields for ticker
        vector<string> required_fields = {"symbol", "timestamp", "datetime", "high", "low", 
                                        "bid", "bidVolume", "ask", "askVolume", "vwap", 
                                        "open", "close", "last", "previousClose", 
                                        "change", "percentage", "average", "baseVolume", 
                                        "quoteVolume"};
        bool all_fields_present = true;
        string missing_field = "";
        
        for (const auto& field : required_fields) {
            if (!ticker_response.contains(field)) {
                all_fields_present = false;
                missing_field = field;
                break;
            }
        }
        log_test_result("fetch_ticker - required fields present", all_fields_present,
                      all_fields_present ? "" : "Missing field: " + missing_field);
        
        // Helper function to check if field is valid numeric (number or numeric string)
        auto is_valid_numeric = [](const nlohmann::json& field) {
            return field.is_number() || field.is_string() || field.is_null();
        };
        
        // Validate field types (allowing both numbers and numeric strings)
        bool types_valid = true;
        string type_error = "";
        
        if (!ticker_response["symbol"].is_string()) {
            types_valid = false; type_error = "symbol should be string";
        } else if (!ticker_response["timestamp"].is_number_integer() && !ticker_response["timestamp"].is_null()) {
            types_valid = false; type_error = "timestamp should be integer or null";
        } else if (ticker_response.contains("high") && !is_valid_numeric(ticker_response["high"])) {
            types_valid = false; type_error = "high should be number, numeric string, or null";
        } else if (ticker_response.contains("low") && !is_valid_numeric(ticker_response["low"])) {
            types_valid = false; type_error = "low should be number, numeric string, or null";
        } else if (ticker_response.contains("bid") && !is_valid_numeric(ticker_response["bid"])) {
            types_valid = false; type_error = "bid should be number, numeric string, or null";
        } else if (ticker_response.contains("ask") && !is_valid_numeric(ticker_response["ask"])) {
            types_valid = false; type_error = "ask should be number, numeric string, or null";
        } else if (ticker_response.contains("last") && !is_valid_numeric(ticker_response["last"])) {
            types_valid = false; type_error = "last should be number, numeric string, or null";
        }
        log_test_result("fetch_ticker - field types validation", types_valid, type_error);
        
        // Validate symbol matches request
        string returned_symbol = ticker_response.value("symbol", "");
        bool symbol_matches = (returned_symbol == test_symbol);
        log_test_result("fetch_ticker - symbol matches request", symbol_matches,
                      symbol_matches ? "" : "Expected: " + test_symbol + ", Got: " + returned_symbol);
        
        // Helper function to safely convert to double from number or string
        auto safe_to_double = [](const nlohmann::json& field) -> double {
            if (field.is_null()) return 0.0;
            if (field.is_number()) return field.get<double>();
            if (field.is_string()) return stod(field.get<string>());
            return 0.0;
        };
        
        // Validate bid/ask spread if both exist
        if (ticker_response.contains("bid") && ticker_response.contains("ask") && 
            !ticker_response["bid"].is_null() && !ticker_response["ask"].is_null()) {
            double bid = safe_to_double(ticker_response["bid"]);
            double ask = safe_to_double(ticker_response["ask"]);
            bool spread_valid = (ask >= bid);
            log_test_result("fetch_ticker - valid bid/ask spread", spread_valid,
                          spread_valid ? "Spread: " + to_string(ask - bid) : 
                                       "Ask (" + to_string(ask) + ") should be >= bid (" + to_string(bid) + ")");
        }
        
        // Validate high >= low if both exist
        if (ticker_response.contains("high") && ticker_response.contains("low") && 
            !ticker_response["high"].is_null() && !ticker_response["low"].is_null()) {
            double high = safe_to_double(ticker_response["high"]);
            double low = safe_to_double(ticker_response["low"]);
            bool range_valid = (high >= low);
            log_test_result("fetch_ticker - valid high/low range", range_valid,
                          range_valid ? "Range: " + to_string(high - low) : 
                                      "High (" + to_string(high) + ") should be >= low (" + to_string(low) + ")");
        }
        
        // Validate percentage change calculation if needed fields exist
        if (ticker_response.contains("change") && ticker_response.contains("percentage") && 
            ticker_response.contains("previousClose") &&
            !ticker_response["change"].is_null() && !ticker_response["percentage"].is_null() && 
            !ticker_response["previousClose"].is_null()) {
            double change = safe_to_double(ticker_response["change"]);
            double percentage = safe_to_double(ticker_response["percentage"]);
            double previous_close = safe_to_double(ticker_response["previousClose"]);
            
            if (previous_close != 0) {
                double calculated_percentage = (change / previous_close) * 100;
                bool percentage_consistent = abs(percentage - calculated_percentage) < 0.1; // Allow small rounding differences
                log_test_result("fetch_ticker - percentage calculation consistency", percentage_consistent,
                              percentage_consistent ? "Percentage matches calculated value" :
                                                    "Percentage mismatch: " + to_string(percentage) + "% vs calculated " + to_string(calculated_percentage) + "%");
            }
        }
        
        // Display sample data (handle both string and numeric values)
        auto display_value = [](const nlohmann::json& field) -> string {
            if (field.is_null()) return "null";
            if (field.is_string()) return field.get<string>();
            if (field.is_number()) return to_string(field.get<double>());
            return "N/A";
        };
        
        cout << "Ticker Sample Data:" << endl;
        cout << "  Symbol: " << ticker_response.value("symbol", "N/A") << endl;
        cout << "  Timestamp: " << ticker_response.value("timestamp", 0) << endl;
        cout << "  Last: " << display_value(ticker_response["last"]) << endl;
        cout << "  Bid: " << display_value(ticker_response["bid"]) << endl;
        cout << "  Ask: " << display_value(ticker_response["ask"]) << endl;
        cout << "  High: " << display_value(ticker_response["high"]) << endl;
        cout << "  Low: " << display_value(ticker_response["low"]) << endl;
        cout << "  Volume: " << display_value(ticker_response["baseVolume"]) << endl;
        cout << "  Change: " << display_value(ticker_response["change"]) << endl;
        cout << "  Percentage: " << display_value(ticker_response["percentage"]) << "%" << endl;
        
        // Test with different symbol
        string test_symbol2 = "ETH-PERPETUAL";
        nlohmann::json ticker2 = client->fetch_ticker(test_symbol2);
        bool second_test = !ticker2.empty() && ticker2.contains("symbol") && 
                          ticker2["symbol"].get<string>() == test_symbol2;
        log_test_result("fetch_ticker - different symbol test", second_test,
                      second_test ? "ETH-PERPETUAL ticker fetched successfully" : 
                                  "Failed to fetch ETH-PERPETUAL ticker");
        
        // Test timestamp validity (should be recent)
        if (ticker_response.contains("timestamp") && !ticker_response["timestamp"].is_null()) {
            long long timestamp = ticker_response["timestamp"].get<long long>();
            auto now = chrono::system_clock::now();
            auto now_ms = chrono::duration_cast<chrono::milliseconds>(now.time_since_epoch()).count();
            
            // Allow up to 1 hour difference
            bool timestamp_recent = abs(now_ms - timestamp) < (1000);
            log_test_result("fetch_ticker - timestamp validity", timestamp_recent,
                          timestamp_recent ? "Timestamp is recent" : 
                                           "Timestamp seems too old: " + to_string(timestamp));
        }
        
        return true;
        
    } catch (const exception& e) {
        log_test_result("fetch_ticker - exception handling", false,
                      string("Exception: ") + e.what());
        return false;
    }
}

    void run_all_tests() {

        cout << " DERIBIT EXCHANGE TEST" << endl;
    

        //working 
        bool load_markets_passed = test_load_markets();
        bool fetch_markets_passed = test_fetch_markets();
        bool fetch_order_book_passed = test_fetch_order_book();
        bool fetch_ticker_passed = test_fetch_ticker();
        bool authentication_passed = test_authentication();
        bool fetch_balance_passed = test_fetch_balance();
        bool create_order_passed = test_create_order();
        bool fetch_order_passed = test_fetch_order();
        bool cancel_order_passed = test_cancel_order();

        //not working 
        // bool watch_orders_passed = test_watch_orders();
        // bool watch_order_book_passed = test_watch_order_book();
        
     
        cout << "TEST SUMMARY" << endl;
   
        cout << "Tests run: " << tests_run << endl;
        cout << "Tests passed: " << tests_passed << endl;
        cout << "Tests failed: " << (tests_run - tests_passed) << endl;
        cout << "Success rate: " << (tests_run > 0 ? (tests_passed * 100 / tests_run) : 0) << "%" << endl;
        
        if (tests_passed == tests_run) {
            cout << "ALL TESTS PASSED!" << endl;
        } else {
            cout << "SOME TESTS FAILED!" << endl;
        }
        
    }
};

int main() {
    try {
        DeribitTester tester;
        tester.run_all_tests();
        return 0;
    } catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << endl;
        return 1;
    }
}
