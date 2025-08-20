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


    

    //Create order testing
    // nlohmann::json create_order=client.create_order(
    //     "BTC_USDT", // symbol
    //     "limit",    // type
    //     "buy",      // side
    //     0.0001,     // amount (minimum trade size)
    //     10000,      // price in USDT (must be multiple of tick_size = 1.0)
    //     {
    //         {"post_only", true}});
    // std::cout << "Create Order: " << create_order.dump(4) << std::endl;

    //Cancel order testing
    // nlohmann::json cancel_order= client.cancel_order("BTC_USDT-325600885"); 
    // std::cout << "Orders: " << cancel_order.dump(4) << std::endl;


    //Fetch order testting
    // nlohmann::json fetching_order = client.fetch_order("BTC_USDT-325600885");
    // std::cout << "Order Details: " << fetching_order.dump(4) << std::endl;

    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "Exiting...\n";
    return 0;
}
