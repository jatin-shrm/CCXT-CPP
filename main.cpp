#include "include/deribit.hpp"
#include <json.hpp>
#include <iostream>
#include <thread>
#include <chrono>

int main()
{
    // Configuration for testnet
    nlohmann::json config = {
        {"apiKey", ""}, // Leave empty for public endpoints like fetch_markets
        {"secret", ""},
        {"password", ""},
        {"is_test", false} // true = testnet, false = mainnet
    };

    // Create Deribit client
    Deribit client(config);

    // Connect and fetch markets
    // client.connect();
    // std::cout << "Connected successfully.\n";

    client.fetch_markets();

    // Keep main thread alive to receive messages
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "Exiting...\n";
    return 0;
}
