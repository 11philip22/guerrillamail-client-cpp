#include "guerrillamail/client.hpp"

#include <string>
#include <utility>

#include "protocol/bootstrap.hpp"
#include "transport/curl_session.hpp"

namespace guerrillamail {

namespace {

constexpr const char* kDefaultBaseUrl = "https://www.guerrillamail.com";
constexpr const char* kDefaultAjaxPath = "/ajax.php";

[[noreturn]] void throw_not_implemented() {
    throw Error(ErrorCode::internal, "not implemented");
}

std::string trim_trailing_slash(std::string value) {
    while (!value.empty() && value.back() == '/') {
        value.pop_back();
    }
    return value;
}

ClientOptions normalize_options(ClientOptions options) {
    if (options.base_url.empty()) {
        options.base_url = kDefaultBaseUrl;
    }

    if (options.ajax_url.empty()) {
        options.ajax_url = trim_trailing_slash(options.base_url) + kDefaultAjaxPath;
    }

    return options;
}

} // namespace

struct Client::Impl {
    Impl(ClientOptions options_in, transport::CurlSession session_in, std::string api_token_in)
        : options(std::move(options_in)), session(std::move(session_in)), api_token(std::move(api_token_in)) {}

    ClientOptions options;
    mutable transport::CurlSession session;
    std::string api_token;
};

Client::Client(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

Client Client::create(const ClientOptions& options) {
    auto normalized = normalize_options(options);

    transport::SessionOptions session_options;
    session_options.timeout = normalized.timeout;
    session_options.proxy = normalized.proxy;
    session_options.verify_tls = normalized.verify_tls;

    auto session = transport::CurlSession(std::move(session_options));
    const auto bootstrap = protocol::bootstrap::perform(session, normalized.base_url);

    return Client(std::make_shared<Impl>(
        std::move(normalized),
        std::move(session),
        bootstrap.api_token
    ));
}

std::string Client::create_email(std::string_view) const {
    throw_not_implemented();
}

std::vector<Message> Client::get_messages(std::string_view) const {
    throw_not_implemented();
}

EmailDetails Client::fetch_email(std::string_view, std::string_view) const {
    throw_not_implemented();
}

void Client::delete_email(std::string_view) const {
    throw_not_implemented();
}

std::vector<Attachment> Client::list_attachments(std::string_view, std::string_view) const {
    throw_not_implemented();
}

std::vector<std::uint8_t> Client::fetch_attachment(
    std::string_view,
    std::string_view,
    const Attachment&
) const {
    throw_not_implemented();
}

} // namespace guerrillamail
