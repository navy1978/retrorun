#include "network_status.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <mutex>
#include <net/if.h>
#include <netdb.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
struct NetworkSnapshot {
    bool checking = false;
    bool checked = false;
    bool online = false;
    std::string interface_name = "None";
    std::string address = "Unavailable";
    std::string error;
    long latency_ms = 0;
    std::time_t checked_at = 0;
};

std::mutex snapshot_mutex;
NetworkSnapshot snapshot;
std::thread worker;

void find_local_address(NetworkSnapshot& value) {
    ifaddrs* interfaces = nullptr;
    if (getifaddrs(&interfaces) != 0) return;
    for (ifaddrs* item = interfaces; item; item = item->ifa_next) {
        if (!item->ifa_addr || item->ifa_addr->sa_family != AF_INET ||
            !(item->ifa_flags & IFF_UP) || (item->ifa_flags & IFF_LOOPBACK))
            continue;
        char address[INET_ADDRSTRLEN] = {};
        const auto* ipv4 = reinterpret_cast<const sockaddr_in*>(item->ifa_addr);
        if (inet_ntop(AF_INET, &ipv4->sin_addr, address, sizeof(address))) {
            value.interface_name = item->ifa_name;
            value.address = address;
            break;
        }
    }
    freeifaddrs(interfaces);
}

bool connect_with_timeout(const addrinfo* address, int timeout_ms, std::string& error) {
    const int fd = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
    if (fd < 0) { error = std::strerror(errno); return false; }
    const int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    int result = connect(fd, address->ai_addr, address->ai_addrlen);
    if (result != 0 && errno == EINPROGRESS) {
        pollfd poll_fd = {fd, POLLOUT, 0};
        result = poll(&poll_fd, 1, timeout_ms);
        if (result > 0) {
            int socket_error = 0;
            socklen_t length = sizeof(socket_error);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error, &length);
            if (socket_error == 0) result = 1;
            else { errno = socket_error; result = -1; }
        }
    } else if (result == 0) {
        result = 1;
    }
    if (result <= 0) error = result == 0 ? "Connection timed out" : std::strerror(errno);
    close(fd);
    return result > 0;
}

void probe() {
    NetworkSnapshot result;
    result.checking = true;
    find_local_address(result);
    const auto started = std::chrono::steady_clock::now();
    addrinfo hints = {};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* addresses = nullptr;
    const int dns_result = getaddrinfo("retroachievements.org", "443", &hints, &addresses);
    if (dns_result != 0) {
        result.error = std::string("DNS: ") + gai_strerror(dns_result);
    } else {
        for (const addrinfo* address = addresses; address; address = address->ai_next) {
            std::string connect_error;
            if (connect_with_timeout(address, 3000, connect_error)) {
                result.online = true;
                result.error.clear();
                break;
            }
            result.error = connect_error;
        }
        freeaddrinfo(addresses);
    }
    result.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - started).count();
    result.checking = false;
    result.checked = true;
    result.checked_at = std::time(nullptr);
    std::lock_guard<std::mutex> lock(snapshot_mutex);
    snapshot = std::move(result);
}

NetworkSnapshot copy_snapshot() {
    std::lock_guard<std::mutex> lock(snapshot_mutex);
    return snapshot;
}
}

void network_status_refresh() {
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex);
        if (snapshot.checking) return;
    }
    if (worker.joinable()) worker.join();
    {
        std::lock_guard<std::mutex> lock(snapshot_mutex);
        snapshot.checking = true;
        snapshot.error.clear();
    }
    worker = std::thread(probe);
}

void network_status_shutdown() {
    if (worker.joinable()) worker.join();
}

std::string network_status_connection_label() {
    const auto value = copy_snapshot();
    if (value.checking) return "Internet: Checking...";
    if (!value.checked) return "Internet: Not checked";
    return value.online ? "Internet: Online" : "Internet: Offline - " + value.error;
}
std::string network_status_interface_label() {
    return "Interface: " + copy_snapshot().interface_name;
}
std::string network_status_address_label() {
    return "IPv4: " + copy_snapshot().address;
}
std::string network_status_latency_label() {
    const auto value = copy_snapshot();
    return value.online ? "RetroAchievements: " + std::to_string(value.latency_ms) + " ms"
                        : "RetroAchievements: Unreachable";
}
std::string network_status_checked_label() {
    const auto value = copy_snapshot();
    if (!value.checked_at) return "Last check: Never";
    char time_text[16] = {};
    std::tm local = {};
    localtime_r(&value.checked_at, &local);
    std::strftime(time_text, sizeof(time_text), "%H:%M:%S", &local);
    return std::string("Last check: ") + time_text;
}
