#include "include/deribit.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    // Configuration for testnet
    nlohmann::json config = {
        {"apiKey", "b42LEgvL"}, // Leave empty for public endpoints like fetch_markets
        {"secret", "FWsNL9GznVOz7x3CIV1lkQ3CrPMtVevzqLBohI4slko"},
        {"password", ""},
        {"is_test", true} // true = testnet, false = mainnet
    };

    Deribit client(config);


    // client.fetch_markets();

    // client.create_order(
    //     "BTC-6AUG25-112000-C", // symbol
    //     "limit",               // type
    //     "buy",                 // side
    //     1,                     // amount (1 contract)
    //     0.01,                  // price (in BTC)
    //     {
    //         {"post_only", true} // optional: avoid taker fees
    //     });
    // nlohmann::json order_book = client.fetch_ticker("BTC-10AUG25-112000-C");
    
    // nlohmann::json create_order=client.create_order(
    //     "BTC_USDT", // symbol
    //     "limit",    // type
    //     "buy",      // side
    //     0.0001,     // amount (minimum trade size)
    //     10000,      // price in USDT (must be multiple of tick_size = 1.0)
    //     {
    //         {"post_only", true}});

    // std::cout << "Create Order: " << create_order.dump(4) << std::endl;
    // nlohmann::json cancel_order= client.cancel_order("BTC_USDT-301468432"); // Replace with actual order ID

    nlohmann::json orders = client.fetch_orders(
        "BTC_USDT", // symbol
        0,          // since
        10,         // limit
        {}          // params
    );
    std::cout << "Orders: " << orders.dump(4) << std::endl;
    // std::cout << "Cancel Order Response: " << cancel_order.dump(4);
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "Exiting...\n";
    return 0;
}
