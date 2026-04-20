#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace guerrillamail::transport {

enum class HttpMethod {
    get,
    post,
};

struct Header {
    std::string name;
    std::string value;
};

struct Request {
    HttpMethod method = HttpMethod::get;
    std::string url;
    std::vector<Header> headers;
    std::string body;
};

struct Response {
    long status_code = 0;
    std::string body;
};

struct SessionOptions {
    std::chrono::milliseconds timeout{30000};
    std::optional<std::string> proxy;
    bool verify_tls = true;
};

class CurlSession {
public:
    explicit CurlSession(SessionOptions options = {});
    ~CurlSession();

    CurlSession(CurlSession&& other) noexcept;
    CurlSession& operator=(CurlSession&& other) noexcept;

    CurlSession(const CurlSession&) = delete;
    CurlSession& operator=(const CurlSession&) = delete;

    [[nodiscard]] Response execute(const Request& request);

private:
    struct Impl;
    Impl* impl_;
};

} // namespace guerrillamail::transport
