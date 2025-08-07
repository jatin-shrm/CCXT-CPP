#pragma once

#include <string>
#include <future>
#include <optional>
#include <json.hpp>

class Exchange
{
public:
    std::string apiKey;
    std::string secret;
    std::string password;

    virtual ~Exchange() = default;


    virtual std::future<nlohmann::json> authenticate() = 0;

    // Async JSON-returning interface
    virtual std::future<nlohmann::json> fetch_markets() = 0;
    virtual std::future<nlohmann::json> fetch_balance() = 0;
    virtual std::future<nlohmann::json> fetch_ticker(const std::string &symbol) = 0;
    virtual std::future<nlohmann::json> fetch_order_book(const std::string &symbol) = 0;

    virtual std::future<nlohmann::json> fetch_orders(
        const std::string &currency = "any",
        const std::string &kind = "any",
        const std::string &order_type = "",
        const std::string &extra_state = "",
        const nlohmann::json &extra_params = {}) = 0;

    virtual std::future<nlohmann::json> create_order(
        const std::string &symbol,
        const std::string &type,
        const std::string &side,
        double amount,
        std::optional<double> price = std::nullopt,
        const nlohmann::json &params = nlohmann::json::object()) = 0;

    virtual std::future<nlohmann::json> cancel_order(const std::string &order_id) = 0;
};
