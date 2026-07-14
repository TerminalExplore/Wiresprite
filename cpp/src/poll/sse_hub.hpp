#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>

namespace wiresprite {

// A minimal wake-up broadcaster for Server-Sent Events: Poller calls
// notify() once per completed poll cycle, and each /api/events HTTP
// connection blocks in waitForChange() until the next notification (or
// a timeout, so it can send an SSE keep-alive comment). Deliberately
// carries no payload — the HTTP layer already knows how to build the
// status JSON (buildStatusJson), so this stays a plain synchronization
// primitive that poll/ can own without depending on http/.
class SseHub {
public:
    // Called by Poller after every poll cycle completes.
    void notify();

    // Called once by HttpServer::stop() before svr_.stop(), so any
    // connections blocked in waitForChange() unblock immediately
    // instead of waiting out a keep-alive timeout.
    void shutdown();

    // Blocks until the generation advances past lastSeen, shutdown()
    // is called, or timeout elapses. Returns the new generation, or
    // nullopt on timeout/shutdown — callers that need to tell the two
    // apart (e.g. to end the stream on shutdown rather than sending a
    // keep-alive) should check isShuttingDown() afterward.
    std::optional<uint64_t> waitForChange(uint64_t lastSeen, std::chrono::milliseconds timeout) const;

    uint64_t currentGeneration() const;
    bool isShuttingDown() const;

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    uint64_t generation_ = 0;
    bool shuttingDown_ = false;
};

} // namespace wiresprite
