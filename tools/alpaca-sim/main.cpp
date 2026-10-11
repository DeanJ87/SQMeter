// Alpaca simulator: serves the firmware's Alpaca API (Alpaca::Router from
// lib/AlpacaLogic) on a desktop machine with fixed, healthy sensor readings,
// so ConformU can test the protocol in CI without an ESP32.
//
//   tools/alpaca-sim/build.sh && ./alpaca-sim [port]     (default 11111)
//
// Deliberately tiny: one request per connection, GET query strings and
// application/x-www-form-urlencoded PUT bodies, nothing else.

#include "AlpacaRouter.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>

using namespace SQM::Alpaca;

namespace
{
    class SimBackend : public Backend
    {
    public:
        bool alpacaEnabled() const override { return true; }
        bool isSafe() const override { return true; }
        ObservingConditionsSnapshot observingConditions() const override
        {
            ObservingConditionsSnapshot s;
            const SourceState fresh{true, true, 2.0};
            s.skyLight = s.irSky = s.environment = s.rain = s.wind = s.windVane = fresh;
            s.cloudCoverPercent = 12.0f;
            s.dewpointC = 4.5f;
            s.humidityPercent = 68.0f;
            s.pressureHPa = 1013.2f;
            s.rainRateMmPerHour = 0.0f;
            s.skyBrightnessLux = 0.0003f;
            s.skyQualityMagArcsec2 = 21.4f;
            s.skyTemperatureC = -24.5f;
            s.temperatureC = 10.5f;
            s.windDirectionDeg = 245.0f;
            s.windGustMs = 4.1f;
            s.windSpeedMs = 2.3f;
            return s;
        }
        std::string serverName() const override { return "SQMeter simulator"; }
        std::string location() const override { return SQM::Alpaca::formatLocation(51.4779, -0.0015); }
        std::string timestampUtc() const override
        {
            const time_t now = time(nullptr);
            struct tm utc;
            gmtime_r(&now, &utc);
            char buffer[32];
            strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
            return buffer;
        }
    };

    std::string urlDecode(const std::string &in)
    {
        std::string out;
        for (size_t i = 0; i < in.size(); ++i)
        {
            if (in[i] == '+')
                out += ' ';
            else if (in[i] == '%' && i + 2 < in.size())
            {
                out += static_cast<char>(std::strtol(in.substr(i + 1, 2).c_str(), nullptr, 16));
                i += 2;
            }
            else
                out += in[i];
        }
        return out;
    }

    void parseParams(const std::string &text, Request &request)
    {
        size_t start = 0;
        while (start < text.size())
        {
            size_t end = text.find('&', start);
            if (end == std::string::npos)
                end = text.size();
            const std::string pair = text.substr(start, end - start);
            const size_t eq = pair.find('=');
            if (!pair.empty())
                request.params.emplace_back(urlDecode(pair.substr(0, eq)), eq == std::string::npos ? "" : urlDecode(pair.substr(eq + 1)));
            start = end + 1;
        }
    }

    void sendAll(int fd, const std::string &data)
    {
        size_t sent = 0;
        while (sent < data.size())
        {
            const ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
            if (n <= 0)
                return;
            sent += static_cast<size_t>(n);
        }
    }

    void serve(int client, Router &router)
    {
        std::string raw;
        char buffer[4096];
        size_t headerEnd = std::string::npos;
        while (headerEnd == std::string::npos)
        {
            const ssize_t n = recv(client, buffer, sizeof(buffer), 0);
            if (n <= 0)
                return;
            raw.append(buffer, static_cast<size_t>(n));
            headerEnd = raw.find("\r\n\r\n");
        }

        size_t contentLength = 0;
        const std::string headers = raw.substr(0, headerEnd);
        for (size_t line = headers.find("\r\n"); line != std::string::npos; line = headers.find("\r\n", line + 2))
        {
            const size_t next = headers.find("\r\n", line + 2);
            std::string header = headers.substr(line + 2, next == std::string::npos ? std::string::npos : next - line - 2);
            for (char &c : header)
                c = static_cast<char>(tolower(c));
            if (header.rfind("content-length:", 0) == 0)
                contentLength = std::strtoul(header.c_str() + 15, nullptr, 10);
        }
        while (raw.size() < headerEnd + 4 + contentLength)
        {
            const ssize_t n = recv(client, buffer, sizeof(buffer), 0);
            if (n <= 0)
                break;
            raw.append(buffer, static_cast<size_t>(n));
        }

        // Request line: METHOD /path?query HTTP/1.1
        const size_t methodEnd = raw.find(' ');
        const size_t targetEnd = raw.find(' ', methodEnd + 1);
        const std::string method = raw.substr(0, methodEnd);
        const std::string target = raw.substr(methodEnd + 1, targetEnd - methodEnd - 1);

        Request request;
        request.get = method == "GET";
        request.put = method == "PUT";
        const size_t question = target.find('?');
        request.path = target.substr(0, question);
        if (question != std::string::npos)
            parseParams(target.substr(question + 1), request);
        if (request.put)
            parseParams(raw.substr(headerEnd + 4, contentLength), request);

        Response response;
        if (!router.handle(request, response))
        {
            response.status = 404;
            response.contentType = "text/plain";
            response.body = "Not found";
        }
        const char *reason = response.status == 200 ? "OK" : response.status == 400 ? "Bad Request" : "Not Found";
        std::string reply = "HTTP/1.1 " + std::to_string(response.status) + " " + reason + "\r\n" +
                            "Content-Type: " + response.contentType + "\r\n" + "Content-Length: " + std::to_string(response.body.size()) +
                            "\r\n" + "Connection: close\r\n\r\n" + response.body;
        sendAll(client, reply);
        std::printf("%s %s -> %d\n", method.c_str(), target.c_str(), response.status);
        std::fflush(stdout);
    }
} // namespace

int main(int argc, char **argv)
{
    const int port = argc > 1 ? std::atoi(argv[1]) : 11111;
    std::signal(SIGPIPE, SIG_IGN);

    const int server = socket(AF_INET, SOCK_STREAM, 0);
    const int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<uint16_t>(port));
    if (bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 || listen(server, 16) != 0)
    {
        std::perror("alpaca-sim: bind/listen");
        return 1;
    }
    std::printf("Alpaca simulator on http://127.0.0.1:%d\n", port);
    std::fflush(stdout);

    SimBackend backend;
    Router router(backend, {"SQMeter (simulator)", "SQMeter", "sim", 0x0000AABBCCDDEEFFULL});
    for (;;)
    {
        const int client = accept(server, nullptr, nullptr);
        if (client < 0)
            continue;
        serve(client, router);
        close(client);
    }
}
