#ifndef MARKET_PUBLISHER_HPP
#define MARKET_PUBLISHER_HPP

#include "Trade.hpp"
#include <memory>
#include <vector>
#include <mutex>
#include <functional>

// Forward declarations to avoid including Session.hpp
class Session;

class MarketPublisher {
public:
    static MarketPublisher& getInstance();

    void registerSession(std::weak_ptr<Session> session);
    void unregisterSession(std::weak_ptr<Session> session);

    void broadcastTradeToMarket(const Trade& trade, std::weak_ptr<Session> sender);

    size_t getSessionCount();

private:
    MarketPublisher() = default;
    ~MarketPublisher() = default;

    // Prevent copying
    MarketPublisher(const MarketPublisher&) = delete;
    MarketPublisher& operator=(const MarketPublisher&) = delete;

    std::mutex sessionsMutex_;
    std::vector<std::weak_ptr<Session>> sessions_;

    void cleanupExpiredSessions();
    std::string formatMarketTrade(const Trade& trade) const;
};

#endif
