#include <iostream>

#include "guerrillamail/client.hpp"

int main() {
    guerrillamail::ClientOptions options;
    auto client = guerrillamail::Client::create(options);

    const auto email = client.create_email("demo");
    std::cout << "Created address: " << email << "\n";

    const auto messages = client.get_messages(email);
    std::cout << "Inbox messages: " << messages.size() << "\n";

    if (!messages.empty()) {
        const auto details = client.fetch_email(email, messages.front().mail_id);
        std::cout << "First message subject: " << details.mail_subject << "\n";

        if (!details.attachments.empty()) {
            const auto bytes = client.fetch_attachment(email, details.mail_id, details.attachments.front());
            std::cout << "Downloaded first attachment bytes: " << bytes.size() << "\n";
        }
    }

    const auto deleted = client.delete_email(email);
    std::cout << "Delete requested: " << std::boolalpha << deleted << "\n";

    return 0;
}
