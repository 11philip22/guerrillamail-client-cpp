#include "guerrillamail/client.hpp"

#include <utility>

namespace guerrillamail {

namespace {

[[noreturn]] void throw_not_implemented() {
    throw Error(ErrorCode::internal, "not implemented");
}

} // namespace

struct Client::Impl {
    explicit Impl(ClientOptions options_in) : options(std::move(options_in)) {}

    ClientOptions options;
};

Client::Client(std::shared_ptr<Impl> impl) : impl_(std::move(impl)) {}

Client Client::create(const ClientOptions& options) {
    return Client(std::make_shared<Impl>(options));
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
