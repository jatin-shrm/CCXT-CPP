#include "include/deribit.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip>
#include <unordered_set>

Deribit::Deribit(const nlohmann::json &config)
{
    apiKey = config.value("apiKey", "");
    secret = config.value("secret", "");
    password = config.value("password", "");
    is_test = config.value("is_test", true);
    url = is_test ? "wss://test.deribit.com/ws/api/v2" : "wss://www.deribit.com/ws/api/v2";

    client.clear_access_channels(websocketpp::log::alevel::all);
    client.clear_error_channels(websocketpp::log::elevel::all);
    client.init_asio();
    client.set_open_handler(std::bind(&Deribit::on_open, this, std::placeholders::_1));
    client.set_message_handler(std::bind(&Deribit::on_message, this, std::placeholders::_1, std::placeholders::_2));
    client.set_fail_handler(std::bind(&Deribit::on_fail, this, std::placeholders::_1));
    client.set_close_handler(std::bind(&Deribit::on_close, this, std::placeholders::_1));
}

Deribit::~Deribit()
{
    if (connected)
    {
        client.close(connection_hdl, websocketpp::close::status::normal, "");
    }
}

void Deribit::connect()
{
    std::unique_lock<std::mutex> lock(mtx);
    if (connected)
    {
        return;
    }

    connection_failed = false;
    client.set_tls_init_handler([](websocketpp::connection_hdl)
                                { return websocketpp::lib::make_shared<boost::asio::ssl::context>(
                                      boost::asio::ssl::context::tlsv12_client); });
    websocketpp::lib::error_code ec;
    auto con = client.get_connection(url, ec);
    if (ec)
    {
        throw std::runtime_error("Connection error: " + ec.message());
    }

    client.connect(con);
    std::thread([this]()
                { client.run(); })
        .detach();

    if (!cv.wait_for(lock, std::chrono::seconds(10), [this]()
                     { return connected || connection_failed; }))
    {
        throw std::runtime_error("Connection timed out");
    }

    if (connection_failed)
    {
        throw std::runtime_error("Connection failed");
    }
}

bool Deribit::is_connected()
{
    std::lock_guard<std::mutex> lock(mtx);
    return connected;
}

void Deribit::on_open(websocketpp::connection_hdl hdl)
{
    connection_hdl = hdl;
    {
        std::lock_guard<std::mutex> lock(mtx);
        connected = true;
        connection_failed = false;
    }
    cv.notify_one();
}

void Deribit::on_message(websocketpp::connection_hdl, message_ptr msg)
{
    std::string payload = msg->get_payload();
    try
    {
        auto response = nlohmann::json::parse(payload);

        if (response.contains("id") && response["id"].is_number_integer())
        {
            int id = response["id"];
            std::lock_guard<std::mutex> lock(pending_requests_mutex);
            auto it = pending_requests.find(id);
            if (it != pending_requests.end())
            {
                ResponseHandler &handler = it->second;
                std::lock_guard<std::mutex> lock_handler(handler.mtx);
                handler.response = response;
                handler.received = true;
                handler.cv.notify_one();
            }
        }
        else if (response.contains("error"))
        {
            std::cerr << "Deribit error: " << response["error"].dump() << std::endl;
        }
    }
    catch (const std::exception &e)
    {
        std::cerr << "Failed to parse message: " << e.what() << std::endl;
    }
}

void Deribit::on_fail(websocketpp::connection_hdl)
{
    std::cerr << "Connection failed" << std::endl;
    {
        std::lock_guard<std::mutex> lock(mtx);
        connected = false;
        connection_failed = true;
    }
    cv.notify_one();
}

void Deribit::on_close(websocketpp::connection_hdl)
{
    std::cerr << "Connection closed" << std::endl;
    {
        std::lock_guard<std::mutex> lock(mtx);
        connected = false;
    }
    cv.notify_one();
}

void Deribit::send_request(const nlohmann::json &request)
{
    if (!is_connected())
    {
        connect();
    }

    websocketpp::lib::error_code ec;
    std::string payload = request.dump();
    client.send(connection_hdl, payload, websocketpp::frame::opcode::text, ec);
    if (ec)
    {
        throw std::runtime_error("Send failed: " + ec.message());
    }
}

nlohmann::json Deribit::send_request_and_wait(const nlohmann::json &request, int timeout_seconds)
{
    int id = request["id"];
    {
        std::lock_guard<std::mutex> lock(pending_requests_mutex);
        pending_requests.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(id),
                                 std::forward_as_tuple());
    }

    try
    {
        send_request(request);
    }
    catch (...)
    {
        std::lock_guard<std::mutex> lock(pending_requests_mutex);
        pending_requests.erase(id);
        throw;
    }

    auto &handler = pending_requests[id];
    std::unique_lock<std::mutex> lock(handler.mtx);
    if (handler.cv.wait_for(lock, std::chrono::seconds(timeout_seconds),
                            [&handler]()
                            { return handler.received; }))
    {
        nlohmann::json resp = handler.response;
        {
            std::lock_guard<std::mutex> lock_map(pending_requests_mutex);
            pending_requests.erase(id);
        }
        return resp;
    }
    else
    {
        std::lock_guard<std::mutex> lock_map(pending_requests_mutex);
        pending_requests.erase(id);
        throw std::runtime_error("Request timed out");
    }
}

std::string Deribit::generate_signature(const std::string &timestamp, const std::string &nonce)
{
    std::string message = timestamp + "\n" + nonce + "\n";
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = EVP_MAX_MD_SIZE;

    HMAC(EVP_sha256(),
         secret.c_str(), secret.length(),
         reinterpret_cast<const unsigned char *>(message.c_str()), message.length(),
         hash, &len);

    std::stringstream ss;
    for (unsigned int i = 0; i < len; ++i)
    {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(hash[i]);
    }
    return ss.str();
}

void Deribit::authenticate()
{
    std::unique_lock<std::mutex> lock(auth_mtx);
    long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();

    if (authenticated && now < auth_expires_at)
    {
        return;
    }

    if (auth_in_progress)
    {
        auth_cv.wait(lock, [this, now]()
                     { return authenticated && now < auth_expires_at; });
        return;
    }

    auth_in_progress = true;
    lock.unlock();

    std::string timestamp = std::to_string(now);
    std::string nonce = timestamp;
    std::string signature = generate_signature(timestamp, nonce);

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/auth"},
        {"params", {{"grant_type", "client_signature"}, {"client_id", apiKey}, {"timestamp", now}, {"nonce", nonce}, {"signature", signature}, {"data", ""}}}};

    try
    {
        auto resp = send_request_and_wait(req, 30);
        std::unique_lock<std::mutex> auth_lock(auth_mtx);
        if (resp.contains("result") && resp["result"].is_object())
        {
            auto result = resp["result"];
            if (result.contains("access_token"))
            {
                access_token = result["access_token"];
                auth_expires_at = now + result.value("expires_in", 0) * 1000;
                authenticated = true;
            }
        }
        else if (resp.contains("error"))
        {
            throw std::runtime_error("Authentication failed: " + resp["error"].dump());
        }
        auth_in_progress = false;
        auth_cv.notify_all();
    }
    catch (const std::exception &e)
    {
        std::unique_lock<std::mutex> auth_lock(auth_mtx);
        authenticated = false;
        auth_in_progress = false;
        auth_cv.notify_all();
        throw;
    }
}

std::string iso8601(int64_t timestamp)
{
    if (timestamp < 0)
    {
        return "";
    }

    try
    {
        std::time_t seconds = timestamp / 1000;
        int milliseconds = timestamp % 1000;

        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&seconds), "%Y-%m-%dT%H:%M:%S")
            << "." << std::setw(3) << std::setfill('0') << milliseconds
            << "Z";

        return oss.str();
    }
    catch (...)
    {
        return "";
    }
}

nlohmann::json Deribit::load_markets(bool reload, const nlohmann::json &params)
{
    if (!reload && !markets.empty() && !markets_by_id.empty())
    {
        return markets;
    }

    nlohmann::json fresh_markets = fetch_markets(params);

    this->markets = fresh_markets;
    this->markets_by_id.clear();

    for (const auto &market : fresh_markets)
    {
        std::string id = market["id"];
        markets_by_id[id] = market;
    }
    return fresh_markets;
}

nlohmann::json Deribit::fetch_markets(const nlohmann::json &params)
{
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/get_instruments"},
        {"params", {{"expired", false}}}};

    nlohmann::json response = send_request_and_wait(req, 30);
    nlohmann::json instruments = response.value("result", nlohmann::json::array());

    nlohmann::json result = nlohmann::json::array();
    std::unordered_set<std::string> parsedMarkets;

    for (auto &market : instruments)
    {
        std::string kind = market.value("kind", "");
        bool isSpot = (kind == "spot");

        std::string id = market.value("instrument_name", "");
        std::string baseId = market.value("base_currency", "");
        std::string quoteId = market.value("counter_currency", "");
        std::string settleId = market.value("settlement_currency", "");
        std::string base = baseId;
        std::string quote = quoteId;
        std::string settle = settleId;

        std::string settlementPeriod = market.value("settlement_period", "");
        bool swap = (settlementPeriod == "perpetual");
        bool future = (!swap && kind.find("future") != std::string::npos);
        bool option = (kind.find("option") != std::string::npos);
        bool isComboMarket = (kind.find("combo") != std::string::npos);

        long long expiry = market.value("expiration_timestamp", 0LL);
        double strike = NAN;
        std::string optionType;

        std::string symbol = id;
        std::string type = "swap";
        if (future)
            type = "future";
        else if (option)
            type = "option";
        else if (isSpot)
            type = "spot";

        if (isSpot)
        {
            symbol = base + "/" + quote;
        }
        else if (!isComboMarket)
        {
            symbol = base + "/" + quote + ":" + settle;
            if (option || future)
            {
                symbol += "-" + std::to_string(expiry);
                if (option)
                {
                    strike = market.value("strike", NAN);
                    optionType = market.value("option_type", "");
                    std::string letter = (optionType == "call") ? "C" : "P";
                    symbol += "-" + std::to_string(strike) + "-" + letter;
                }
            }
        }

        if (parsedMarkets.count(symbol))
        {
            continue;
        }
        parsedMarkets.insert(symbol);

        double minTradeAmount = market.value("min_trade_amount", NAN);
        double tickSize = market.value("tick_size", NAN);

        nlohmann::json precision = {
            {"amount", minTradeAmount},
            {"price", tickSize}};

        nlohmann::json limits = {
            {"leverage", {{"min", nullptr}, {"max", nullptr}}},
            {"amount", {{"min", minTradeAmount}, {"max", nullptr}}},
            {"price", {{"min", tickSize}, {"max", nullptr}}},
            {"cost", {{"min", nullptr}, {"max", nullptr}}}};

        nlohmann::json parsedMarket = {
            {"id", id},
            {"symbol", symbol},
            {"base", base},
            {"quote", quote},
            {"settle", settle},
            {"baseId", baseId},
            {"quoteId", quoteId},
            {"settleId", settleId},
            {"type", type},
            {"spot", isSpot},
            {"margin", false},
            {"swap", swap},
            {"future", future},
            {"option", option},
            {"active", market.value("is_active", true)},
            {"contract", !isSpot},
            {"linear", (settle == quote)},
            {"inverse", (settle != quote)},
            {"taker", market.value("taker_commission", NAN)},
            {"maker", market.value("maker_commission", NAN)},
            {"contractSize", market.value("contract_size", NAN)},
            {"expiry", expiry},
            {"expiryDatetime", expiry > 0 ? std::to_string(expiry) : ""},
            {"strike", std::isnan(strike) ? nullptr : nlohmann::json(strike)},
            {"optionType", optionType.empty() ? nullptr : nlohmann::json(optionType)},
            {"precision", precision},
            {"limits", limits},
            {"created", market.value("creation_timestamp", 0LL)},
            {"info", market}};

        result.push_back(parsedMarket);
    }

    return result;
}

nlohmann::json Deribit::fetch_balance(const nlohmann::json &params)
{
    authenticate();

    std::string currencyCode = "BTC";
    if (params.contains("code") && params["code"].is_string())
    {
        currencyCode = params["code"].get<std::string>();
    }

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/get_account_summary"},
        {"params", {{"currency", currencyCode}}}};

    nlohmann::json response = send_request_and_wait(req, 30);
    nlohmann::json balance = response.value("result", nlohmann::json::object());

    nlohmann::json result;
    result["info"] = balance;

    std::string currencyId = balance.value("currency", "");

    nlohmann::json account;
    account["free"] = balance.value("available_funds", 0.0);
    account["used"] = balance.value("maintenance_margin", 0.0);
    account["total"] = balance.value("equity", 0.0);

    result[currencyCode] = account;

    return result;
}

nlohmann::json Deribit::fetch_orders(const std::string &symbol, const std::string &currency, const std::string &kind, const std::string &interval, const nlohmann::json &extra_params)
{
    authenticate();
    nlohmann::json params = {
        {"currency", currency},
        {"kind", kind}};

    if (!symbol.empty())
        params["instrument_name"] = symbol;
    for (auto it = extra_params.begin(); it != extra_params.end(); ++it)
    {
        params[it.key()] = it.value();
    }

    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "private/get_order_history_by_currency"},
        {"params", params}};
    return send_request_and_wait(req, 30);
}

nlohmann::json Deribit::fetch_ticker(const std::string &symbol)
{
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/ticker"},
        {"params", {{"instrument_name", symbol}}}};

    nlohmann::json response = send_request_and_wait(req, 30);
    nlohmann::json ticker = response.value("result", nlohmann::json::object());

    int64_t timestamp = 0;
    if (ticker.contains("timestamp") && ticker["timestamp"].is_number_integer())
        timestamp = ticker["timestamp"].get<int64_t>();
    else if (ticker.contains("creation_timestamp") && ticker["creation_timestamp"].is_number_integer())
        timestamp = ticker["creation_timestamp"].get<int64_t>();

    std::string last;
    if (ticker.contains("last_price") && !ticker["last_price"].is_null())
        last = std::to_string(ticker["last_price"].get<double>());
    else if (ticker.contains("last") && !ticker["last"].is_null())
        last = std::to_string(ticker["last"].get<double>());

    nlohmann::json stats = ticker.contains("stats") && ticker["stats"].is_object()
                               ? ticker["stats"]
                               : ticker;

    std::string high;
    if (stats.contains("high") && !stats["high"].is_null())
        high = std::to_string(stats["high"].get<double>());
    else if (stats.contains("max_price") && !stats["max_price"].is_null())
        high = std::to_string(stats["max_price"].get<double>());

    std::string low;
    if (stats.contains("low") && !stats["low"].is_null())
        low = std::to_string(stats["low"].get<double>());
    else if (stats.contains("min_price") && !stats["min_price"].is_null())
        low = std::to_string(stats["min_price"].get<double>());

    std::string bid;
    if (ticker.contains("best_bid_price") && !ticker["best_bid_price"].is_null())
        bid = std::to_string(ticker["best_bid_price"].get<double>());
    else if (ticker.contains("bid_price") && !ticker["bid_price"].is_null())
        bid = std::to_string(ticker["bid_price"].get<double>());

    std::string ask;
    if (ticker.contains("best_ask_price") && !ticker["best_ask_price"].is_null())
        ask = std::to_string(ticker["best_ask_price"].get<double>());
    else if (ticker.contains("ask_price") && !ticker["ask_price"].is_null())
        ask = std::to_string(ticker["ask_price"].get<double>());

    std::string bidVolume;
    if (ticker.contains("best_bid_amount") && !ticker["best_bid_amount"].is_null())
        bidVolume = std::to_string(ticker["best_bid_amount"].get<double>());

    std::string askVolume;
    if (ticker.contains("best_ask_amount") && !ticker["best_ask_amount"].is_null())
        askVolume = std::to_string(ticker["best_ask_amount"].get<double>());

    std::string quoteVolume;
    if (stats.contains("volume") && !stats["volume"].is_null())
        quoteVolume = std::to_string(stats["volume"].get<double>());

    nlohmann::json result;
    result["symbol"] = symbol;
    result["timestamp"] = timestamp;
    result["datetime"] = timestamp ? nlohmann::json(iso8601(timestamp)) : nlohmann::json();
    result["high"] = high.empty() ? nlohmann::json() : nlohmann::json(high);
    result["low"] = low.empty() ? nlohmann::json() : nlohmann::json(low);
    result["bid"] = bid.empty() ? nlohmann::json() : nlohmann::json(bid);
    result["bidVolume"] = bidVolume.empty() ? nlohmann::json() : nlohmann::json(bidVolume);
    result["ask"] = ask.empty() ? nlohmann::json() : nlohmann::json(ask);
    result["askVolume"] = askVolume.empty() ? nlohmann::json() : nlohmann::json(askVolume);
    result["vwap"] = nlohmann::json();
    result["open"] = nlohmann::json();
    result["close"] = last.empty() ? nlohmann::json() : nlohmann::json(last);
    result["last"] = last.empty() ? nlohmann::json() : nlohmann::json(last);
    result["previousClose"] = nlohmann::json();
    result["change"] = nlohmann::json();
    result["percentage"] = nlohmann::json();
    result["average"] = nlohmann::json();
    result["baseVolume"] = nlohmann::json();
    result["quoteVolume"] = quoteVolume.empty() ? nlohmann::json() : nlohmann::json(quoteVolume);
    result["info"] = ticker;

    return result;
}

nlohmann::json Deribit::fetch_order_book(const std::string &symbol, const nlohmann::json &params)
{
    nlohmann::json req = {
        {"jsonrpc", "2.0"},
        {"id", request_id++},
        {"method", "public/get_order_book"},
        {"params", {{"instrument_name", symbol}, {"depth", 5}}}};

    nlohmann::json response = send_request_and_wait(req, 30);
    nlohmann::json orderbook = response.value("result", nlohmann::json::object());

    auto parse_bid_ask = [&](const nlohmann::json &bidask, int priceKey = 0, int amountKey = 1, int countOrIdKey = 2)
    {
        nlohmann::json entry = nlohmann::json::array();
        double price = (bidask.is_array() && bidask.size() > priceKey && !bidask[priceKey].is_null())
                           ? bidask[priceKey].get<double>()
                           : 0.0;
        double amount = (bidask.is_array() && bidask.size() > amountKey && !bidask[amountKey].is_null())
                            ? bidask[amountKey].get<double>()
                            : 0.0;
        entry.push_back(price);
        entry.push_back(amount);
        if (bidask.is_array() && bidask.size() > countOrIdKey && !bidask[countOrIdKey].is_null())
        {
            entry.push_back(bidask[countOrIdKey]);
        }
        return entry;
    };

    auto parse_bids_asks = [&](const nlohmann::json &bidasks)
    {
        nlohmann::json result = nlohmann::json::array();
        if (bidasks.is_array())
        {
            for (auto &bidask : bidasks)
            {
                result.push_back(parse_bid_ask(bidask));
            }
        }
        return result;
    };

    nlohmann::json bids = parse_bids_asks(orderbook.value("bids", nlohmann::json::array()));
    nlohmann::json asks = parse_bids_asks(orderbook.value("asks", nlohmann::json::array()));

    auto sort_desc = [](nlohmann::json arr)
    {
        std::sort(arr.begin(), arr.end(), [](const nlohmann::json &a, const nlohmann::json &b)
                  { return a[0].get<double>() > b[0].get<double>(); });
        return arr;
    };

    auto sort_asc = [](nlohmann::json arr)
    {
        std::sort(arr.begin(), arr.end(), [](const nlohmann::json &a, const nlohmann::json &b)
                  { return a[0].get<double>() < b[0].get<double>(); });
        return arr;
    };

    bids = sort_desc(bids);
    asks = sort_asc(asks);

    std::cout << orderbook.dump(4) << std::endl;

    int64_t ts = orderbook.value("timestamp", int64_t(0));
    if (ts > 0 && ts < 10000000000LL)
    {
        ts *= 1000;
    }

    nlohmann::json result;
    result["symbol"] = symbol;
    result["bids"] = bids;
    result["asks"] = asks;
    result["timestamp"] = ts;
    result["datetime"] = ts ? nlohmann::json(iso8601(ts)) : nlohmann::json();
    result["nonce"] = nullptr;

    return result;
}

nlohmann::json Deribit::create_order(const std::string &symbol, const std::string &type, const std::string &side, double amount, std::optional<double> price, const nlohmann::json &params)
{
    authenticate();
    nlohmann::json req;
    req["jsonrpc"] = "2.0";
    req["id"] = request_id++;
    req["method"] = side == "buy" ? "private/buy" : "private/sell";

    nlohmann::json order_params;
    order_params["instrument_name"] = symbol;
    order_params["amount"] = amount;

    std::string trigger = params.value("trigger", "last_price");
    std::string timeInForce = params.value("timeInForce", "");
    bool reduceOnly = params.value("reduceOnly", false);
    bool postOnly = params.value("postOnly", false);
    auto trailingAmountIt = params.find("trailingAmount");
    auto stopLossPriceIt = params.find("stopLossPrice");
    auto takeProfitPriceIt = params.find("takeProfitPrice");
    bool hasStopLoss = stopLossPriceIt != params.end() && !stopLossPriceIt->is_null();
    bool hasTakeProfit = takeProfitPriceIt != params.end() && !takeProfitPriceIt->is_null();

    if (hasStopLoss && hasTakeProfit)
        throw std::runtime_error("Cannot specify both stopLossPrice and takeProfitPrice");

    if (type == "limit" && price.has_value())
    {
        order_params["type"] = "limit";
        order_params["price"] = price.value();
    }
    else if (type == "market")
    {
        order_params["type"] = "market";
    }

    if (trailingAmountIt != params.end() && !trailingAmountIt->is_null())
    {
        order_params["type"] = "trailing_stop";
        order_params["trigger"] = trigger;
        order_params["trigger_offset"] = std::stod(trailingAmountIt->get<std::string>());
    }
    else if (hasStopLoss || hasTakeProfit)
    {
        double triggerPrice = hasStopLoss ? stopLossPriceIt->get<double>() : takeProfitPriceIt->get<double>();
        order_params["trigger"] = trigger;
        order_params["trigger_price"] = triggerPrice;
        if (hasStopLoss)
            order_params["type"] = type == "market" ? "stop_market" : "stop_limit";
        else
            order_params["type"] = type == "market" ? "take_market" : "take_limit";
    }

    if (reduceOnly)
        order_params["reduce_only"] = true;
    if (postOnly)
    {
        order_params["post_only"] = true;
        order_params["reject_post_only"] = true;
    }
    if (!timeInForce.empty())
    {
        if (timeInForce == "GTC")
            order_params["time_in_force"] = "good_til_cancelled";
        else if (timeInForce == "IOC")
            order_params["time_in_force"] = "immediate_or_cancel";
        else if (timeInForce == "FOK")
            order_params["time_in_force"] = "fill_or_kill";
    }

    req["params"] = order_params;
    nlohmann::json response = send_request_and_wait(req, 30);

    nlohmann::json order = response["result"]["order"];
    nlohmann::json trades = response["result"].contains("trades") ? response["result"]["trades"] : nlohmann::json::array();
    std::string marketId = order.value("instrument_name", "");
    int64_t timestamp = order.value("creation_timestamp", 0);
    int64_t lastUpdate = order.value("last_update_timestamp", 0);
    std::string id = order.value("order_id", "");
    std::string priceString = order.contains("price") && !order["price"].is_null() ? order["price"].dump() : "";
    if (priceString == "\"market_price\"")
        priceString.clear();
    std::string averageString = order.contains("average_price") && !order["average_price"].is_null() ? order["average_price"].dump() : "";
    std::string filledString = order.contains("filled_amount") && !order["filled_amount"].is_null() ? order["filled_amount"].dump() : "";
    std::string amountString = order.contains("amount") && !order["amount"].is_null() ? order["amount"].dump() : "";
    std::string cost;
    if (!filledString.empty() && !averageString.empty())
        cost = std::to_string(std::stod(filledString) * std::stod(averageString));
    int64_t lastTradeTimestamp = 0;
    if (!filledString.empty() && std::stod(filledString) > 0)
        lastTradeTimestamp = lastUpdate;
    std::string status = order.value("order_state", "");
    std::string sideParsed = order.value("direction", "");
    std::string feeCostString = order.contains("commission") && !order["commission"].is_null() ? order["commission"].dump() : "";
    nlohmann::json fee = nlohmann::json();
    if (!feeCostString.empty())
    {
        fee["cost"] = std::abs(std::stod(feeCostString));
        fee["currency"] = "";
    }
    std::string rawType = order.value("order_type", "");
    std::string timeInForceParsed = order.value("time_in_force", "");
    auto stopPrice = order.contains("stop_price") ? order["stop_price"] : nlohmann::json();
    bool postOnlyParsed = order.value("post_only", false);

    nlohmann::json parsed;
    parsed["info"] = order;
    parsed["id"] = id;
    parsed["clientOrderId"] = nlohmann::json();
    parsed["timestamp"] = timestamp;
    parsed["datetime"] = timestamp ? nlohmann::json(iso8601(timestamp)) : nlohmann::json();
    parsed["lastTradeTimestamp"] = lastTradeTimestamp ? nlohmann::json(lastTradeTimestamp) : nlohmann::json();
    parsed["symbol"] = marketId;
    parsed["type"] = rawType;
    parsed["timeInForce"] = timeInForceParsed;
    parsed["postOnly"] = postOnlyParsed;
    parsed["side"] = sideParsed;
    parsed["price"] = priceString.empty() ? nlohmann::json() : nlohmann::json(priceString);
    parsed["stopPrice"] = stopPrice.is_null() ? nlohmann::json() : stopPrice;
    parsed["triggerPrice"] = stopPrice.is_null() ? nlohmann::json() : stopPrice;
    parsed["amount"] = amountString.empty() ? nlohmann::json() : nlohmann::json(amountString);
    parsed["cost"] = cost.empty() ? nlohmann::json() : nlohmann::json(cost);
    parsed["average"] = averageString.empty() ? nlohmann::json() : nlohmann::json(averageString);
    parsed["filled"] = filledString.empty() ? nlohmann::json() : nlohmann::json(filledString);
    parsed["remaining"] = nlohmann::json();
    parsed["status"] = status;
    parsed["fee"] = fee.is_null() ? nlohmann::json() : fee;
    parsed["trades"] = trades;

    return parsed;
}

nlohmann::json Deribit::cancel_order(const std::string &id, const std::string &symbol, const nlohmann::json &params)
{
    load_markets(false, {});

    authenticate();
    nlohmann::json req;
    req["jsonrpc"] = "2.0";
    req["id"] = request_id++;
    req["method"] = "private/cancel";
    req["params"] = {{"order_id", id}};
    for (auto &el : params.items())
    {
        req["params"][el.key()] = el.value();
    }

    nlohmann::json response = send_request_and_wait(req, 30);
    nlohmann::json order = response.value("result", nlohmann::json::object());

    std::string marketId = order.value("instrument_name", "");
    int64_t timestamp = order.value("creation_timestamp", 0);
    int64_t lastUpdate = order.value("last_update_timestamp", 0);
    std::string orderId = order.value("order_id", "");
    std::string priceString = order.contains("price") && !order["price"].is_null() ? order["price"].dump() : "";
    if (priceString == "\"market_price\"")
        priceString.clear();
    std::string averageString = order.contains("average_price") && !order["average_price"].is_null() ? order["average_price"].dump() : "";
    std::string filledString = order.contains("filled_amount") && !order["filled_amount"].is_null() ? order["filled_amount"].dump() : "";
    std::string amountString = order.contains("amount") && !order["amount"].is_null() ? order["amount"].dump() : "";
    std::string cost;
    if (!filledString.empty() && !averageString.empty())
        cost = std::to_string(std::stod(filledString) * std::stod(averageString));
    int64_t lastTradeTimestamp = 0;
    if (!filledString.empty() && std::stod(filledString) > 0)
        lastTradeTimestamp = lastUpdate;
    std::string status = order.value("order_state", "");
    std::string side = order.value("direction", "");
    std::string feeCostString = order.contains("commission") && !order["commission"].is_null() ? order["commission"].dump() : "";
    nlohmann::json fee = nlohmann::json();
    if (!feeCostString.empty())
    {
        fee["cost"] = std::abs(std::stod(feeCostString));
        fee["currency"] = "";
    }
    std::string rawType = order.value("order_type", "");
    std::string timeInForceParsed = order.value("time_in_force", "");
    auto stopPrice = order.contains("stop_price") ? order["stop_price"] : nlohmann::json();
    bool postOnlyParsed = order.value("post_only", false);
    nlohmann::json trades = order.contains("trades") ? order["trades"] : nlohmann::json::array();

    nlohmann::json parsed;
    parsed["info"] = order;
    parsed["id"] = orderId;
    parsed["clientOrderId"] = nlohmann::json();
    parsed["timestamp"] = timestamp;
    parsed["datetime"] = timestamp ? nlohmann::json(iso8601(timestamp)) : nlohmann::json();
    parsed["lastTradeTimestamp"] = lastTradeTimestamp ? nlohmann::json(lastTradeTimestamp) : nlohmann::json();
    parsed["symbol"] = marketId;
    parsed["type"] = rawType;
    parsed["timeInForce"] = timeInForceParsed;
    parsed["postOnly"] = postOnlyParsed;
    parsed["side"] = side;
    parsed["price"] = priceString.empty() ? nlohmann::json() : nlohmann::json(priceString);
    parsed["triggerPrice"] = stopPrice.is_null() ? nlohmann::json() : stopPrice;
    parsed["amount"] = amountString.empty() ? nlohmann::json() : nlohmann::json(amountString);
    parsed["cost"] = cost.empty() ? nlohmann::json() : nlohmann::json(cost);
    parsed["average"] = averageString.empty() ? nlohmann::json() : nlohmann::json(averageString);
    parsed["filled"] = filledString.empty() ? nlohmann::json() : nlohmann::json(filledString);
    parsed["remaining"] = nlohmann::json();
    parsed["status"] = status;
    parsed["fee"] = fee.is_null() ? nlohmann::json() : fee;
    parsed["trades"] = trades;

    return parsed;
}
