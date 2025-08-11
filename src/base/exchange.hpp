#pragma once

#include <string>

class Exchange
{
public:
    std::string apiKey;
    std::string secret;
    std::string password;

    virtual ~Exchange() = default;

    virtual void authenticate() = 0;

    virtual nlohmann::json load_markets(bool reload = false, const nlohmann::json &params = nlohmann::json::object()) = 0;
    virtual nlohmann::json fetch_markets(const nlohmann::json &params = nlohmann::json::object()) = 0;
    virtual nlohmann::json fetch_balance(const nlohmann::json &params = nlohmann::json::object()) = 0;
    virtual nlohmann::json fetch_ticker(const std::string &symbol) = 0;
    virtual nlohmann::json fetch_order_book(const std::string &symbol, const nlohmann::json &params = nlohmann::json::object()) = 0;
    virtual nlohmann::json fetch_orders(const std::string &symbol = "", const std::string &currency = "any", const std::string &kind = "any", const std::string &interval = "raw", const nlohmann::json &extra_params = {}) = 0;
    virtual nlohmann::json create_order(const std::string &symbol, const std::string &type, const std::string &side, double amount, std::optional<double> price = std::nullopt, const nlohmann::json &params = nlohmann::json::object()) = 0;
    virtual nlohmann::json cancel_order(const std::string &order_id) = 0;
};
