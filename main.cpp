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

    // Create order testing
    //  nlohmann::json create_order=client.create_order(
    //      "BTC_USDT", // symbol
    //      "limit",    // type
    //      "buy",      // side
    //      0.0001,     // amount (minimum trade size)
    //      10000,      // price in USDT (must be multiple of tick_size = 1.0)
    //      {
    //          {"post_only", true}});
    //  std::cout << "Create Order: " << create_order.dump(4) << std::endl;

    // Cancel order testing
    //  nlohmann::json cancel_order= client.cancel_order("BTC_USDT-325600885");
    //  std::cout << "Orders: " << cancel_order.dump(4) << std::endl;

    // Fetch order testting
    //  nlohmann::json fetching_order = client.fetch_order("BTC_USDT-325600885");
    //  std::cout << "Order Details: " << fetching_order.dump(4) << std::endl;

    // Define a handler function that prints order updates for watch orders
    // auto orderHandler = [](const nlohmann::json &order)
    // {
    //     std::cout << "Order Update Received:\n";
    //     std::cout << "ID: " << order.value("order_id", "N/A") << "\n";
    //     std::cout << "Instrument: " << order.value("instrument_name", "N/A") << "\n";
    //     std::cout << "Direction: " << order.value("direction", "N/A") << "\n";
    //     std::cout << "Price: " << order.value("price", 0.0) << "\n";
    //     std::cout << "Amount: " << order.value("amount", 0.0) << "\n";
    //     std::cout << "Filled: " << order.value("filled_amount", 0.0) << "\n";
    //     std::cout << "State: " << order.value("order_state", "N/A") << "\n";
    //     std::cout << "----------------------------------------\n";
    // };

    // try
    // {
    //     // Subscribe to order updates
    //     client.watch_orders(orderHandler);

    //     std::cout << "Subscribed to order updates. Waiting for messages...\n";

    //     // Keep the program running to receive updates
    //     while (true)
    //     {
    //         std::this_thread::sleep_for(std::chrono::seconds(1));
    //     }
    // }
    // catch (const std::exception &e)
    // {
    //     std::cerr << "Error: " << e.what() << std::endl;
    // }

    
    // Define a handler function that prints order book updates
    auto orderBookHandler = [](const nlohmann::json &message)
    {
        std::cout << "Order Book Update Received:\n";

        // Sometimes the feed gives "data", sometimes bids/asks directly
        const nlohmann::json *data = nullptr;
        if (message.contains("data"))
            data = &message["data"];
        else
            data = &message;

        // instrument_name / change_id / timestamp may exist only in "data"
        if (data->contains("instrument_name"))
            std::cout << "Instrument: " << data->value("instrument_name", "N/A") << "\n";
        if (data->contains("change_id"))
            std::cout << "Change ID: " << data->value("change_id", 0) << "\n";
        if (data->contains("timestamp"))
            std::cout << "Timestamp: " << data->value("timestamp", 0) << "\n";

        // Handle bids
        if (data->contains("bids"))
        {
            std::cout << "Top Bids:\n";
            for (auto &bid : (*data)["bids"])
            {
                // ["new", price, amount]
                std::string action = bid[0].get<std::string>();
                double price = bid[1].get<double>();
                double amount = bid[2].get<double>();

                std::cout << "  Action: " << action
                          << " | Price: " << price
                          << " | Amount: " << amount << "\n";
            }
        }

        // Handle asks
        if (data->contains("asks"))
        {
            std::cout << "Top Asks:\n";
            for (auto &ask : (*data)["asks"])
            {
                std::string action = ask[0].get<std::string>();
                double price = ask[1].get<double>();
                double amount = ask[2].get<double>();

                std::cout << "  Action: " << action
                          << " | Price: " << price
                          << " | Amount: " << amount << "\n";
            }
        }

        std::cout << "----------------------------------------\n";
    };

    // Parameters for subscription
    nlohmann::json params = {
        {"interval", "100ms"},
        {"useDepthEndpoint", false}};

    // Subscribe to the order book for BTC-PERPETUAL
    client.watch_order_book(orderBookHandler, "BTC-PERPETUAL", 20, params);

    std::this_thread::sleep_for(std::chrono::seconds(1000));

    // std::this_thread::sleep_for(std::chrono::seconds(1000));

    std::cout << "Exiting...\n";
    return 0;
}
