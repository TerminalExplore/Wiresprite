#pragma once

#include <cstdint>
#include <string>

// Thin cross-platform UDP wrapper (Winsock2 on Windows, BSD sockets
// elsewhere). Knows nothing about SNMP — just resolves a host:port,
// sends datagrams, and receives with a timeout.
namespace wiresprite {

class UdpSocket {
public:
#ifdef _WIN32
    using SocketHandle = std::uintptr_t; // SOCKET
#else
    using SocketHandle = int;
#endif

    UdpSocket();
    ~UdpSocket();
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // Binds the local end to `localPort` (0 = let the OS pick an
    // ephemeral port). Only needed by a listener (e.g. a test fake
    // agent); a plain client can skip this and let sendTo's implicit
    // bind-on-first-send pick a port.
    void bind(uint16_t localPort);

    // Resolves `host` (dotted IPv4 or hostname) and sends `data` to it.
    // Throws std::runtime_error if resolution or the send fails.
    void sendTo(const std::string& host, uint16_t port, const std::string& data);

    // Waits up to timeoutMs for a datagram. Returns false on timeout
    // (not an error). Throws std::runtime_error on a hard socket error.
    bool receiveFrom(std::string& outData, std::string& fromHost, uint16_t& fromPort, int timeoutMs);

    // The local port the OS assigned (e.g. after bind(0)), for tests
    // that need to tell a client where a loopback listener ended up.
    uint16_t localPort() const;

private:
    SocketHandle fd_;
};

} // namespace wiresprite
