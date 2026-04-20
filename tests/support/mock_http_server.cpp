#include "support/mock_http_server.hpp"

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace guerrillamail::tests::support {

namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle invalid_socket_handle = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle invalid_socket_handle = -1;
#endif

struct SocketRuntime {
    SocketRuntime() {
#ifdef _WIN32
        WSADATA data{};
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }

    ~SocketRuntime() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

SocketRuntime& socket_runtime() {
    static SocketRuntime runtime;
    return runtime;
}

void close_socket(SocketHandle socket_handle) {
#ifdef _WIN32
    closesocket(socket_handle);
#else
    close(socket_handle);
#endif
}

std::string trim_copy(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.erase(value.begin());
    }

    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.pop_back();
    }

    return value;
}

std::string to_lower_copy(std::string_view input) {
    std::string output(input);
    std::transform(output.begin(), output.end(), output.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return output;
}

std::string status_text(int status_code) {
    switch (status_code) {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 503:
        return "Service Unavailable";
    default:
        return "Status";
    }
}

MockHttpRequest parse_request(std::string request_text) {
    MockHttpRequest request;

    const auto header_end = request_text.find("\r\n\r\n");
    const auto head = request_text.substr(0, header_end);
    request.body = header_end == std::string::npos ? std::string() : request_text.substr(header_end + 4);

    std::istringstream stream(head);
    std::string request_line;
    std::getline(stream, request_line);
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream request_line_stream(request_line);
    request_line_stream >> request.method >> request.path;

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (line.empty()) {
            continue;
        }

        const auto separator = line.find(':');
        if (separator == std::string::npos) {
            continue;
        }

        request.headers.emplace_back(
            trim_copy(line.substr(0, separator)),
            trim_copy(line.substr(separator + 1))
        );
    }

    return request;
}

std::string read_request(SocketHandle client_socket) {
    std::string request_text;
    std::array<char, 4096> buffer{};
    std::size_t expected_size = 0;

    for (;;) {
#ifdef _WIN32
        const auto bytes_read = recv(client_socket, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
        const auto bytes_read = recv(client_socket, buffer.data(), buffer.size(), 0);
#endif
        if (bytes_read <= 0) {
            break;
        }

        request_text.append(buffer.data(), static_cast<std::size_t>(bytes_read));

        const auto header_end = request_text.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            continue;
        }

        if (expected_size == 0) {
            const auto content_length_pos = to_lower_copy(request_text.substr(0, header_end)).find("content-length:");
            if (content_length_pos != std::string::npos) {
                const auto value_start = content_length_pos + std::strlen("content-length:");
                const auto line_end = request_text.find("\r\n", value_start);
                const auto value = trim_copy(request_text.substr(value_start, line_end - value_start));
                expected_size = static_cast<std::size_t>(std::stoul(value));
            }
        }

        const auto body_size = request_text.size() - (header_end + 4);
        if (body_size >= expected_size) {
            break;
        }
    }

    return request_text;
}

void send_response(SocketHandle client_socket, const MockHttpResponse& response) {
    std::ostringstream stream;
    stream << "HTTP/1.1 " << response.status_code << ' ' << status_text(response.status_code) << "\r\n";
    stream << "Content-Length: " << response.body.size() << "\r\n";
    stream << "Connection: close\r\n";
    for (const auto& header : response.headers) {
        stream << header.first << ": " << header.second << "\r\n";
    }
    stream << "\r\n";
    stream << response.body;

    const auto payload = stream.str();
    const char* current = payload.data();
    std::size_t remaining = payload.size();

    while (remaining > 0) {
#ifdef _WIN32
        const auto sent = send(client_socket, current, static_cast<int>(remaining), 0);
#else
        const auto sent = send(client_socket, current, remaining, 0);
#endif
        if (sent <= 0) {
            break;
        }

        current += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
}

} // namespace

std::optional<std::string> MockHttpRequest::header_value(std::string_view name) const {
    const auto expected = to_lower_copy(name);
    for (const auto& header : headers) {
        if (to_lower_copy(header.first) == expected) {
            return header.second;
        }
    }

    return std::nullopt;
}

struct MockHttpServer::Impl {
    explicit Impl(Handler handler_in) : handler(std::move(handler_in)) {
        (void)socket_runtime();

        listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_socket == invalid_socket_handle) {
            throw std::runtime_error("failed to create listen socket");
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;

        if (bind(listen_socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
            close_socket(listen_socket);
            throw std::runtime_error("failed to bind mock server socket");
        }

        if (listen(listen_socket, 8) != 0) {
            close_socket(listen_socket);
            throw std::runtime_error("failed to listen on mock server socket");
        }

        sockaddr_in bound_address{};
#ifdef _WIN32
        int bound_address_size = sizeof(bound_address);
#else
        socklen_t bound_address_size = sizeof(bound_address);
#endif
        getsockname(listen_socket, reinterpret_cast<sockaddr*>(&bound_address), &bound_address_size);
        port = ntohs(bound_address.sin_port);

        server_thread = std::thread([this]() { run(); });
    }

    ~Impl() {
        stop_requested = true;
        if (listen_socket != invalid_socket_handle) {
            close_socket(listen_socket);
        }
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }

    void run() {
        while (!stop_requested) {
            sockaddr_in client_address{};
#ifdef _WIN32
            int client_address_size = sizeof(client_address);
#else
            socklen_t client_address_size = sizeof(client_address);
#endif
            const auto client_socket = accept(
                listen_socket,
                reinterpret_cast<sockaddr*>(&client_address),
                &client_address_size
            );

            if (client_socket == invalid_socket_handle) {
                if (stop_requested) {
                    break;
                }
                continue;
            }

            const auto request_text = read_request(client_socket);
            const auto request = parse_request(request_text);
            const auto response = handler(request);

            if (response.delay.count() > 0) {
                std::this_thread::sleep_for(response.delay);
            }

            send_response(client_socket, response);
            close_socket(client_socket);
        }
    }

    Handler handler;
    SocketHandle listen_socket = invalid_socket_handle;
    std::atomic<bool> stop_requested = false;
    std::thread server_thread;
    unsigned short port = 0;
};

MockHttpServer::MockHttpServer(Handler handler) : impl_(new Impl(std::move(handler))) {}

MockHttpServer::~MockHttpServer() {
    delete impl_;
}

std::string MockHttpServer::url(std::string_view path, std::string_view scheme) const {
    std::ostringstream stream;
    stream << scheme << "://127.0.0.1:" << impl_->port;
    if (path.empty()) {
        stream << '/';
    } else if (path.front() == '/') {
        stream << path;
    } else {
        stream << '/' << path;
    }
    return stream.str();
}

} // namespace guerrillamail::tests::support
