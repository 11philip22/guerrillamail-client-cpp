#include "guerrillamail/client.hpp"

#include <chrono>
#include <string>
#include <utility>

#include "protocol/bootstrap.hpp"
#include "protocol/parsing.hpp"
#include "protocol/requests.hpp"
#include "transport/curl_session.hpp"

namespace guerrillamail {

namespace {

constexpr const char* kDefaultBaseUrl = "https://www.guerrillamail.com";
constexpr const char* kDefaultAjaxPath = "/ajax.php";

std::string make_timestamp() {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
}

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

std::string Client::create_email(std::string_view alias) const {
    if (alias.find('@') != std::string_view::npos) {
        throw Error(ErrorCode::invalid_argument, "alias must not contain `@`");
    }

    const auto site_override = impl_->options.site.has_value()
                                   ? std::optional<std::string_view>(impl_->options.site.value())
                                   : std::nullopt;

    const auto response = impl_->session.execute(protocol::requests::build_set_email_user_request(
        impl_->options.ajax_url,
        impl_->api_token,
        alias,
        site_override
    ));
    const auto json = protocol::parsing::parse_json(response.body);
    return protocol::parsing::require_string_member(json, "email_addr");
}

std::vector<Message> Client::get_messages(std::string_view email) const {
    const auto site_override = impl_->options.site.has_value()
                                   ? std::optional<std::string_view>(impl_->options.site.value())
                                   : std::nullopt;

    const auto response = impl_->session.execute(protocol::requests::build_check_email_probe_request(
        impl_->options.ajax_url,
        impl_->api_token,
        email,
        make_timestamp(),
        site_override
    ));
    const auto json = protocol::parsing::parse_json(response.body);
    return protocol::parsing::parse_message_list(json);
}

EmailDetails Client::fetch_email(std::string_view email, std::string_view mail_id) const {
    if (mail_id.empty()) {
        throw Error(ErrorCode::invalid_argument, "mail_id must not be empty");
    }

    const auto site_override = impl_->options.site.has_value()
                                   ? std::optional<std::string_view>(impl_->options.site.value())
                                   : std::nullopt;

    const auto response = impl_->session.execute(protocol::requests::build_fetch_email_request(
        impl_->options.ajax_url,
        impl_->api_token,
        email,
        mail_id,
        make_timestamp(),
        site_override
    ));
    const auto json = protocol::parsing::parse_json(response.body);
    return protocol::parsing::parse_email_details(json);
}

bool Client::delete_email(std::string_view email) const {
    const auto site_override = impl_->options.site.has_value()
                                   ? std::optional<std::string_view>(impl_->options.site.value())
                                   : std::nullopt;

    (void)impl_->session.execute(protocol::requests::build_forget_me_request(
        impl_->options.ajax_url,
        impl_->api_token,
        email,
        site_override
    ));

    return true;
}

std::vector<Attachment> Client::list_attachments(std::string_view email, std::string_view mail_id) const {
    return fetch_email(email, mail_id).attachments;
}

std::vector<std::uint8_t> Client::fetch_attachment(
    std::string_view email,
    std::string_view mail_id,
    const Attachment& attachment
) const {
    if (mail_id.empty()) {
        throw Error(ErrorCode::invalid_argument, "mail_id must not be empty");
    }
    if (attachment.part_id.empty()) {
        throw Error(ErrorCode::invalid_argument, "attachment missing part_id");
    }

    const auto details = fetch_email(email, mail_id);
    const auto sid_token = details.sid_token.has_value() && !details.sid_token->empty()
                               ? std::optional<std::string_view>(details.sid_token.value())
                               : std::nullopt;

    const auto response = impl_->session.execute(protocol::requests::build_fetch_attachment_request(
        impl_->options.base_url,
        impl_->api_token,
        mail_id,
        attachment.part_id,
        sid_token
    ));

    return std::vector<std::uint8_t>(response.body.begin(), response.body.end());
}

} // namespace guerrillamail
