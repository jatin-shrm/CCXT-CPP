#pragma once

#include <json.hpp>
#include <string>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <random>
#include "types.hpp"
#include "exceptions.hpp"

namespace ccxt {

class Utils {
public:
    // Safe JSON parsing utilities
    static std::string safe_string(const nlohmann::json& obj, const std::string& key, 
                                 const std::string& default_val = "") {
        if (obj.contains(key) && !obj[key].is_null()) {
            if (obj[key].is_string()) {
                return obj[key].get<std::string>();
            } else if (obj[key].is_number()) {
                return std::to_string(obj[key].get<double>());
            }
        }
        return default_val;
    }
    
    static double safe_float(const nlohmann::json& obj, const std::string& key, double default_val = 0.0) {
        if (obj.contains(key) && !obj[key].is_null()) {
            if (obj[key].is_string()) {
                try {
                    std::string str_val = obj[key].get<std::string>();
                    if (str_val.empty()) return default_val;
                    return std::stod(str_val);
                } catch (const std::exception&) {
                    return default_val;
                }
            } else if (obj[key].is_number()) {
                return obj[key].get<double>();
            }
        }
        return default_val;
    }
    
    static int64_t safe_integer(const nlohmann::json& obj, const std::string& key, int64_t default_val = 0) {
        if (obj.contains(key) && !obj[key].is_null()) {
            if (obj[key].is_string()) {
                try {
                    std::string str_val = obj[key].get<std::string>();
                    if (str_val.empty()) return default_val;
                    return std::stoll(str_val);
                } catch (const std::exception&) {
                    return default_val;
                }
            } else if (obj[key].is_number()) {
                return obj[key].get<int64_t>();
            }
        }
        return default_val;
    }
    
    static bool safe_bool(const nlohmann::json& obj, const std::string& key, bool default_val = false) {
        if (obj.contains(key) && !obj[key].is_null()) {
            if (obj[key].is_boolean()) {
                return obj[key].get<bool>();
            } else if (obj[key].is_string()) {
                std::string str_val = to_lower(obj[key].get<std::string>());
                return str_val == "true" || str_val == "1" || str_val == "yes";
            } else if (obj[key].is_number()) {
                return obj[key].get<double>() != 0.0;
            }
        }
        return default_val;
    }
    
    static int64_t safe_timestamp(const nlohmann::json& obj, const std::string& key, int64_t default_val = 0) {
        int64_t timestamp = safe_integer(obj, key, default_val);
        
        // Handle different timestamp formats (seconds vs milliseconds)
        if (timestamp > 0) {
            // If timestamp is in seconds (less than year 3000 in milliseconds)
            if (timestamp < 32503680000000LL) {
                timestamp *= 1000; // Convert to milliseconds
            }
        }
        
        return timestamp;
    }
    
    // Time conversion utilities
    static std::chrono::system_clock::time_point timestamp_to_timepoint(int64_t timestamp_ms) {
        return std::chrono::system_clock::time_point(std::chrono::milliseconds(timestamp_ms));
    }
    
    static int64_t timepoint_to_timestamp(const std::chrono::system_clock::time_point& tp) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
    }
    
    static int64_t now_timestamp() {
        return timepoint_to_timestamp(std::chrono::system_clock::now());
    }
    
    // String utilities
    static std::string to_upper(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::toupper);
        return result;
    }
    
    static std::string to_lower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
    
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    // Symbol normalization utilities
    static std::string normalize_symbol(const std::string& symbol) {
        // Convert exchange-specific symbols to unified format
        // Example: "BTC-PERPETUAL" -> "BTC/USD:USD"
        // This is exchange-specific and would need customization
        return symbol;
    }
    
    static std::pair<std::string, std::string> split_symbol(const std::string& symbol) {
        // Split "BTC/USDT" into base="BTC", quote="USDT"
        size_t pos = symbol.find('/');
        if (pos != std::string::npos) {
            return {symbol.substr(0, pos), symbol.substr(pos + 1)};
        }
        
        // Handle other formats like "BTCUSDT"
        // This would need more sophisticated parsing
        return {symbol, ""};
    }
    
    // Precision utilities
    static double round_to_precision(double value, int precision) {
        if (precision <= 0) return value;
        
        double multiplier = std::pow(10.0, precision);
        return std::round(value * multiplier) / multiplier;
    }
    
    static std::string format_price(double price, int precision = 8) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << price;
        std::string result = oss.str();
        
        // Remove trailing zeros after decimal point
        if (result.find('.') != std::string::npos) {
            result = result.substr(0, result.find_last_not_of('0') + 1);
            if (result.back() == '.') {
                result.pop_back();
            }
        }
        
        return result;
    }
    
    // Validation utilities
    static bool is_valid_symbol(const std::string& symbol) {
        return !symbol.empty() && symbol.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789/-_:") == std::string::npos;
    }
    
    static bool is_valid_order_side(const std::string& side) {
        std::string lower_side = to_lower(side);
        return lower_side == "buy" || lower_side == "sell";
    }
    
    static bool is_valid_order_type(const std::string& type) {
        std::string lower_type = to_lower(type);
        return lower_type == "market" || lower_type == "limit" || 
               lower_type == "stop" || lower_type == "stop_limit";
    }
    
    // Error handling utilities
    static void validate_required_field(const nlohmann::json& obj, const std::string& field) {
        if (!obj.contains(field) || obj[field].is_null()) {
            throw MissingFieldError("Required field '" + field + "' is missing or null");
        }
    }
    
    static void validate_positive_number(double value, const std::string& field_name) {
        if (value <= 0.0) {
            throw InvalidOrder(field_name + " must be positive, got: " + std::to_string(value));
        }
    }
    
    static void validate_non_negative_number(double value, const std::string& field_name) {
        if (value < 0.0) {
            throw InvalidOrder(field_name + " must be non-negative, got: " + std::to_string(value));
        }
    }
    
    // URL encoding utility
    static std::string url_encode(const std::string& value) {
        std::ostringstream escaped;
        escaped.fill('0');
        escaped << std::hex;
        
        for (char c : value) {
            if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                escaped << c;
            } else {
                escaped << std::uppercase;
                escaped << '%' << std::setw(2) << int((unsigned char) c);
                escaped << std::nouppercase;
            }
        }
        
        return escaped.str();
    }
    
    // UUID generation for client order IDs
    static std::string generate_uuid() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        static std::uniform_int_distribution<> dis2(8, 11);
        
        std::stringstream ss;
        int i;
        ss << std::hex;
        for (i = 0; i < 8; i++) {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 4; i++) {
            ss << dis(gen);
        }
        ss << "-4";
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }
        ss << "-";
        ss << dis2(gen);
        for (i = 0; i < 3; i++) {
            ss << dis(gen);
        }
        ss << "-";
        for (i = 0; i < 12; i++) {
            ss << dis(gen);
        }
        return ss.str();
    }
};

} // namespace ccxt