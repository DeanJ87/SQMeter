#include "DualStackClient.h"

#include "Logger.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <lwip/netdb.h>
#include <lwip/sockets.h>

namespace SQM
{
    namespace
    {
        constexpr const char *TAG = "Net";
    }

    int DualStackClient::connect(const char *host, uint16_t port)
    {
        return connect(host, port, DEFAULT_TIMEOUT_MS);
    }

    int DualStackClient::connect(const char *hostText, uint16_t port, int32_t timeoutMs)
    {
        // HTTPClient passes URL hosts as written: "[fd00::10]".
        std::string host(hostText);
        if (host.size() > 2 && host.front() == '[' && host.back() == ']')
            host = host.substr(1, host.size() - 2);
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        struct addrinfo *found = nullptr;
        if (lwip_getaddrinfo(host.c_str(), nullptr, &hints, &found) != 0 || found == nullptr)
        {
            Logger::warn(TAG, "Can't resolve %s", host.c_str());
            return 0;
        }

        int result = 0;
        if (found->ai_family == AF_INET)
        {
            const auto *v4 = reinterpret_cast<const struct sockaddr_in *>(found->ai_addr);
            const IPAddress address(v4->sin_addr.s_addr);
            lwip_freeaddrinfo(found);
            return WiFiClient::connect(address, port, timeoutMs);
        }
        if (found->ai_family == AF_INET6)
        {
            struct sockaddr_in6 v6;
            memcpy(&v6, found->ai_addr, sizeof(v6));
            v6.sin6_port = htons(port);
            result = connectIpv6(v6, timeoutMs);
        }
        lwip_freeaddrinfo(found);
        return result;
    }

    int DualStackClient::connectIpv6(const ::sockaddr_in6 &address, int32_t timeoutMs)
    {
        stop();
        const int fd = lwip_socket(AF_INET6, SOCK_STREAM, 0);
        if (fd < 0)
            return 0;
        lwip_fcntl(fd, F_SETFL, lwip_fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

        struct timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;
        int error = 0;
        if (lwip_connect(fd, reinterpret_cast<const struct sockaddr *>(&address), sizeof(address)) < 0 && errno != EINPROGRESS)
        {
            error = errno;
        }
        else
        {
            fd_set writable;
            FD_ZERO(&writable);
            FD_SET(fd, &writable);
            const int ready = lwip_select(fd + 1, nullptr, &writable, nullptr, &timeout);
            socklen_t length = sizeof(error);
            if (ready <= 0)
                error = ready == 0 ? ETIMEDOUT : errno;
            else if (lwip_getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0)
                error = errno;
        }
        if (error != 0)
        {
            Logger::warn(TAG, "IPv6 connect failed: %s", strerror(error));
            lwip_close(fd);
            return 0;
        }

        lwip_setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        lwip_setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        lwip_fcntl(fd, F_SETFL, lwip_fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
        // WiFiClient takes over the connected socket (and closes it in stop()).
        WiFiClient::operator=(WiFiClient(fd));
        return 1;
    }
} // namespace SQM
