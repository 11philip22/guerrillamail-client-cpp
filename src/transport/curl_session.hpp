#ifndef GUERRILLAMAIL_TRANSPORT_CURL_SESSION_HPP
#define GUERRILLAMAIL_TRANSPORT_CURL_SESSION_HPP

#include <curl/curl.h>

#include <chrono>
#include <memory>
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

    [[nodiscard]] std::string execute(const Request& request);

private:
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> handle_{nullptr, curl_easy_cleanup};
};

} // namespace guerrillamail::transport

#endif // GUERRILLAMAIL_TRANSPORT_CURL_SESSION_HPP
