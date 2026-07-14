#include "poll/sse_hub.hpp"

namespace wiresprite {

void SseHub::notify() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++generation_;
    }
    cv_.notify_all();
}

void SseHub::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shuttingDown_ = true;
    }
    cv_.notify_all();
}

std::optional<uint64_t> SseHub::waitForChange(uint64_t lastSeen, std::chrono::milliseconds timeout) const {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock, timeout, [this, lastSeen] { return shuttingDown_ || generation_ > lastSeen; });
    if (shuttingDown_ || generation_ <= lastSeen) {
        return std::nullopt;
    }
    return generation_;
}

uint64_t SseHub::currentGeneration() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return generation_;
}

bool SseHub::isShuttingDown() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return shuttingDown_;
}

} // namespace wiresprite
