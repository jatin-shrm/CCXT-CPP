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

    // Create Deribit client
    Deribit client(config);

    // Connect and fetch markets
    // client.connect();
    // std::cout << "Connected successfully.\n";

    std::cout<<"Fetching markets.\n";
    client.fetch_markets();

    
    // client.create_order("BTC-PERPETUAL", "buy", 10, 20000);

    // client.fetch_orders("BTC");
    // client.fetch_balance();

    // Keep main thread alive to receive messages
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "Exiting...\n";
    return 0;
}
