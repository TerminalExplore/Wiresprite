#include "snmp/udp_socket.hpp"

#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace snmpmon {

namespace {

#ifdef _WIN32
// Winsock requires WSAStartup before any socket call and WSACleanup once
// done; both are internally reference-counted by ws2_32.dll, so it's safe
// for this to run alongside another WSAStartup/WSACleanup pair elsewhere
// in the process. Keeping it self-contained here means UdpSocket works
// correctly regardless of who constructs one (the app, or a test).
struct WinsockGuard {
    WinsockGuard() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("UdpSocket: WSAStartup failed");
        }
    }
    ~WinsockGuard() { WSACleanup(); }
};

void ensureWinsockInitialized() {
    static WinsockGuard guard;
    (void)guard;
}
#endif

} // namespace

UdpSocket::UdpSocket() {
#ifdef _WIN32
    ensureWinsockInitialized();
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) {
        throw std::runtime_error("UdpSocket: socket() failed, WSAGetLastError=" + std::to_string(WSAGetLastError()));
    }

    // Windows-only quirk (KB263823): an ICMP Port Unreachable for a
    // previous sendto() otherwise surfaces as WSAECONNRESET on this
    // socket's *next* recvfrom(), even though UDP is connectionless and
    // that should just mean "nothing arrived yet". Disabling it here
    // makes an unreachable destination behave like a plain timeout,
    // consistent with this class's contract and with Linux's behavior
    // for the same scenario on an unconnected socket.
    BOOL newBehavior = FALSE;
    DWORD bytesReturned = 0;
    WSAIoctl(s, SIO_UDP_CONNRESET, &newBehavior, sizeof(newBehavior), nullptr, 0, &bytesReturned, nullptr, nullptr);

    fd_ = static_cast<SocketHandle>(s);
#else
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) {
        throw std::runtime_error(std::string("UdpSocket: socket() failed: ") + std::strerror(errno));
    }
    fd_ = s;
#endif
}

UdpSocket::~UdpSocket() {
#ifdef _WIN32
    closesocket(static_cast<SOCKET>(fd_));
#else
    close(fd_);
#endif
}

void UdpSocket::bind(uint16_t localPort) {
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(localPort);

#ifdef _WIN32
    int result = ::bind(static_cast<SOCKET>(fd_), reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
#else
    int result = ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
#endif
    if (result != 0) {
        throw std::runtime_error("UdpSocket::bind failed for port " + std::to_string(localPort));
    }
}

void UdpSocket::sendTo(const std::string& host, uint16_t port, const std::string& data) {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    addrinfo* resolved = nullptr;
    int rc = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &resolved);
    if (rc != 0 || resolved == nullptr) {
        throw std::runtime_error("UdpSocket::sendTo: failed to resolve host \"" + host + "\"");
    }

#ifdef _WIN32
    int sent = ::sendto(static_cast<SOCKET>(fd_), data.data(), static_cast<int>(data.size()), 0,
                         resolved->ai_addr, static_cast<int>(resolved->ai_addrlen));
#else
    ssize_t sent = ::sendto(fd_, data.data(), data.size(), 0, resolved->ai_addr, resolved->ai_addrlen);
#endif
    freeaddrinfo(resolved);

    if (sent < 0 || static_cast<size_t>(sent) != data.size()) {
        throw std::runtime_error("UdpSocket::sendTo: sendto() failed for host \"" + host + "\"");
    }
}

uint16_t UdpSocket::localPort() const {
    sockaddr_in addr{};
#ifdef _WIN32
    int addrLen = sizeof(addr);
    if (getsockname(static_cast<SOCKET>(fd_), reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0) {
        throw std::runtime_error("UdpSocket::localPort: getsockname failed");
    }
#else
    socklen_t addrLen = sizeof(addr);
    if (getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0) {
        throw std::runtime_error("UdpSocket::localPort: getsockname failed");
    }
#endif
    return ntohs(addr.sin_port);
}

bool UdpSocket::receiveFrom(std::string& outData, std::string& fromHost, uint16_t& fromPort, int timeoutMs) {
#ifdef _WIN32
    SOCKET s = static_cast<SOCKET>(fd_);
#else
    int s = fd_;
#endif

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(s, &readSet);

    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int selectResult = select(static_cast<int>(s) + 1, &readSet, nullptr, nullptr, &tv);
    if (selectResult == 0) {
        return false; // timeout
    }
    if (selectResult < 0) {
        throw std::runtime_error("UdpSocket::receiveFrom: select() failed");
    }

    char buffer[65536];
    sockaddr_in fromAddr{};
#ifdef _WIN32
    int fromLen = sizeof(fromAddr);
    int received = ::recvfrom(s, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
#else
    socklen_t fromLen = sizeof(fromAddr);
    ssize_t received = ::recvfrom(s, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);
#endif
    if (received < 0) {
        throw std::runtime_error("UdpSocket::receiveFrom: recvfrom() failed");
    }

    outData.assign(buffer, static_cast<size_t>(received));

    char hostBuf[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &fromAddr.sin_addr, hostBuf, sizeof(hostBuf));
    fromHost = hostBuf;
    fromPort = ntohs(fromAddr.sin_port);
    return true;
}

} // namespace snmpmon
