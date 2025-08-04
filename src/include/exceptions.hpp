#pragma once

#include <stdexcept>
#include <string>

namespace ccxt {

// Base CCXT exception class
class CCXTException : public std::exception {
protected:
    std::string message_;
public:
    explicit CCXTException(const std::string& msg) : message_(msg) {}
    const char* what() const noexcept override { return message_.c_str(); }
    const std::string& message() const { return message_; }
};

// Base class for all network-related errors
class NetworkError : public CCXTException {
public:
    explicit NetworkError(const std::string& msg) : CCXTException("Network Error: " + msg) {}
};

// Connection timeout or request timeout
class RequestTimeout : public NetworkError {
public:
    explicit RequestTimeout(const std::string& msg) : NetworkError("Request Timeout: " + msg) {}
};

// Connection failed to establish
class ConnectionError : public NetworkError {
public:
    explicit ConnectionError(const std::string& msg) : NetworkError("Connection Error: " + msg) {}
};

// DNS resolution failed
class DnsError : public NetworkError {
public:
    explicit DnsError(const std::string& msg) : NetworkError("DNS Error: " + msg) {}
};

// SSL/TLS errors
class SSLError : public NetworkError {
public:
    explicit SSLError(const std::string& msg) : NetworkError("SSL Error: " + msg) {}
};

// Base class for all exchange-related errors
class ExchangeError : public CCXTException {
public:
    explicit ExchangeError(const std::string& msg) : CCXTException("Exchange Error: " + msg) {}
};

// Authentication failed
class AuthenticationError : public ExchangeError {
public:
    explicit AuthenticationError(const std::string& msg) : ExchangeError("Authentication Failed: " + msg) {}
};

// API key/secret invalid or expired
class InvalidCredentials : public AuthenticationError {
public:
    explicit InvalidCredentials(const std::string& msg) : AuthenticationError("Invalid Credentials: " + msg) {}
};

// Permission denied for requested operation
class PermissionDenied : public AuthenticationError {
public:
    explicit PermissionDenied(const std::string& msg) : AuthenticationError("Permission Denied: " + msg) {}
};

// Rate limit exceeded
class RateLimitExceeded : public ExchangeError {
public:
    explicit RateLimitExceeded(const std::string& msg) : ExchangeError("Rate Limit Exceeded: " + msg) {}
};

// Exchange is down for maintenance
class ExchangeNotAvailable : public ExchangeError {
public:
    explicit ExchangeNotAvailable(const std::string& msg) : ExchangeError("Exchange Not Available: " + msg) {}
};

// Base class for trading-related errors
class TradingError : public ExchangeError {
public:
    explicit TradingError(const std::string& msg) : ExchangeError("Trading Error: " + msg) {}
};

// Insufficient funds to place order
class InsufficientFunds : public TradingError {
public:
    explicit InsufficientFunds(const std::string& msg) : TradingError("Insufficient Funds: " + msg) {}
};

// Order parameters are invalid
class InvalidOrder : public TradingError {
public:
    explicit InvalidOrder(const std::string& msg) : TradingError("Invalid Order: " + msg) {}
};

// Order not found when trying to cancel/modify
class OrderNotFound : public TradingError {
public:
    explicit OrderNotFound(const std::string& msg) : TradingError("Order Not Found: " + msg) {}
};

// Order already canceled or filled
class OrderNotCancelable : public TradingError {
public:
    explicit OrderNotCancelable(const std::string& msg) : TradingError("Order Not Cancelable: " + msg) {}
};

// Order amount is below minimum
class OrderAmountTooSmall : public InvalidOrder {
public:
    explicit OrderAmountTooSmall(const std::string& msg) : InvalidOrder("Order Amount Too Small: " + msg) {}
};

// Order amount is above maximum
class OrderAmountTooLarge : public InvalidOrder {
public:
    explicit OrderAmountTooLarge(const std::string& msg) : InvalidOrder("Order Amount Too Large: " + msg) {}
};

// Invalid trading pair/symbol
class InvalidSymbol : public TradingError {
public:
    explicit InvalidSymbol(const std::string& msg) : TradingError("Invalid Symbol: " + msg) {}
};

// Market is closed for trading
class MarketClosed : public TradingError {
public:
    explicit MarketClosed(const std::string& msg) : TradingError("Market Closed: " + msg) {}
};

// Position-related errors
class PositionError : public TradingError {
public:
    explicit PositionError(const std::string& msg) : TradingError("Position Error: " + msg) {}
};

// Insufficient margin to maintain position
class InsufficientMargin : public PositionError {
public:
    explicit InsufficientMargin(const std::string& msg) : PositionError("Insufficient Margin: " + msg) {}
};

// Position not found
class PositionNotFound : public PositionError {
public:
    explicit PositionNotFound(const std::string& msg) : PositionError("Position Not Found: " + msg) {}
};

// Base class for data parsing errors
class ParseError : public CCXTException {
public:
    explicit ParseError(const std::string& msg) : CCXTException("Parse Error: " + msg) {}
};

// JSON parsing failed
class JsonParseError : public ParseError {
public:
    explicit JsonParseError(const std::string& msg) : ParseError("JSON Parse Error: " + msg) {}
};

// Expected field missing from response
class MissingFieldError : public ParseError {
public:
    explicit MissingFieldError(const std::string& msg) : ParseError("Missing Field: " + msg) {}
};

// Configuration errors
class ConfigError : public CCXTException {
public:
    explicit ConfigError(const std::string& msg) : CCXTException("Configuration Error: " + msg) {}
};

// Missing required configuration
class MissingConfig : public ConfigError {
public:
    explicit MissingConfig(const std::string& msg) : ConfigError("Missing Configuration: " + msg) {}
};

// Invalid configuration value
class InvalidConfig : public ConfigError {
public:
    explicit InvalidConfig(const std::string& msg) : ConfigError("Invalid Configuration: " + msg) {}
};

// Generic not implemented error
class NotImplemented : public CCXTException {
public:
    explicit NotImplemented(const std::string& msg) : CCXTException("Not Implemented: " + msg) {}
};

// Helper function to map Deribit error codes to appropriate exceptions
CCXTException create_exception_from_deribit_error(int error_code, const std::string& error_message);

// Helper function to map HTTP status codes to appropriate exceptions
CCXTException create_exception_from_http_status(int status_code, const std::string& response_body);

} // namespace ccxt