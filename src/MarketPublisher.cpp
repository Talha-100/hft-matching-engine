#include "MarketPublisher.hpp"
#include "Session.hpp"  // Include the actual Session header
#include <iostream>
#include <algorithm>
#include <sstream>

MarketPublisher& MarketPublisher::getInstance() {
    static MarketPublisher instance;
    return instance;
}

void MarketPublisher::registerSession(std::weak_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_.emplace_back(session);
}

void MarketPublisher::unregisterSession(std::weak_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    auto sessionPtr = session.lock();
    if (!sessionPtr) return;

    sessions_.erase(
        std::remove_if(sessions_.begin(), sessions_.end(),
            [&sessionPtr](const std::weak_ptr<Session>& weakSession) {
                auto sharedSession = weakSession.lock();
                return !sharedSession || sharedSession == sessionPtr;
            }),
        sessions_.end()
    );
}

void MarketPublisher::broadcastTradeToMarket(const Trade& trade, std::weak_ptr<Session> sender) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);

    // Clean up expired sessions first
    cleanupExpiredSessions();

    std::string marketMessage = formatMarketTrade(trade);
    auto senderPtr = sender.lock();

    // Broadcast to all sessions except the sender
    for (const auto& weakSession : sessions_) {
        if (auto session = weakSession.lock()) {
            if (session != senderPtr) {
                session->sendMessage(marketMessage);
            }
        }
    }
}

void MarketPublisher::cleanupExpiredSessions() {
    sessions_.erase(
        std::remove_if(sessions_.begin(), sessions_.end(),
            [](const std::weak_ptr<Session>& weakSession) {
                return weakSession.expired();
            }),
        sessions_.end()
    );
}

std::string MarketPublisher::formatMarketTrade(const Trade& trade) const {
    std::ostringstream oss;
    oss << "MARKET TRADE Price: " << trade.price
        << ", Quantity: " << trade.quantity << "\n\n";
    return oss.str();
}

size_t MarketPublisher::getSessionCount() {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    cleanupExpiredSessions();
    return sessions_.size();
}
