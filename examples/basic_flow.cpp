#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "guerrillamail/client.hpp"
#include "guerrillamail/error.hpp"

int main() {
    try {
        std::cout << "GuerrillaMail C++ Client - Full Demo\n";
        std::cout << std::string(50, '=') << "\n";

        // =========================================
        // 1. Create client (optionally with proxy)
        // =========================================
        std::cout << "\nCreating client...\n";

        guerrillamail::ClientOptions options;

        // Optional configuration examples:
        // options.proxy = std::string("http://127.0.0.1:8080");
        // options.verify_tls = true;
        // options.ajax_url = "https://www.guerrillamail.com/ajax.php";

        auto client = guerrillamail::Client::create(options);
        std::cout << "   Connected to GuerrillaMail API\n";

        // =========================================
        // 2. Create temporary email address
        // =========================================
        std::cout << "\nCreating temporary email...\n";
        const auto alias = std::string("demo") + std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            )
                .count()
        );
        const auto email = client.create_email(alias);
        std::cout << "   Created: " << email << "\n";

        // =========================================
        // 3. Poll for messages (get_messages)
        // =========================================
        std::cout << "\nWaiting for messages...\n";
        std::cout << "   Send an email to: " << email << "\n";
        std::cout << "   (Polling for up to 2 minutes)\n";

        const auto start = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::seconds(120);
        const auto poll_interval = std::chrono::seconds(5);

        while (true) {
            const auto messages = client.get_messages(email);

            if (!messages.empty()) {
                std::cout << "\n\nReceived " << messages.size() << " message(s)!\n";

                for (const auto& message : messages) {
                    std::cout << "\n" << std::string(50, '-') << "\n";
                    std::cout << "Message ID:  " << message.mail_id << "\n";
                    std::cout << "From:        " << message.mail_from << "\n";
                    std::cout << "Subject:     " << message.mail_subject << "\n";
                    std::cout << "Excerpt:     "
                              << message.mail_excerpt.substr(0, std::min<std::size_t>(80, message.mail_excerpt.size()))
                              << "\n";
                    std::cout << "Timestamp:   " << message.mail_timestamp << "\n";

                    // =========================================
                    // 4. Fetch full email content (fetch_email)
                    // =========================================
                    std::cout << "\nFetching full email body...\n";
                    try {
                        const auto details = client.fetch_email(email, message.mail_id);
                        std::cout << "   Body length: " << details.mail_body.size() << " characters\n";
                        std::cout << "   Preview (first 500 chars):\n";
                        std::cout << "   " << std::string(40, '-') << "\n";

                        const auto preview = details.mail_body.substr(0, std::min<std::size_t>(500, details.mail_body.size()));
                        std::cout << preview << "\n";
                        if (details.mail_body.size() > 500) {
                            std::cout << "   ... (truncated)\n";
                        }

                        // =========================================
                        // 4b. Download attachments (if any)
                        // =========================================
                        if (!details.attachments.empty()) {
                            std::cout << "\nFound " << details.attachments.size() << " attachment(s)\n";
                            for (const auto& attachment : details.attachments) {
                                std::cout << "   - " << attachment.filename << "\n";
                                try {
                                    const auto bytes = client.fetch_attachment(email, message.mail_id, attachment);
                                    std::cout << "     Downloaded " << bytes.size() << " bytes\n";
                                } catch (const guerrillamail::Error& error) {
                                    std::cerr << "     Download failed: " << error.what() << "\n";
                                }
                            }
                        }
                    } catch (const guerrillamail::Error& error) {
                        std::cerr << "   Failed to fetch: " << error.what() << "\n";
                    }
                }

                break;
            }

            if (std::chrono::steady_clock::now() - start >= timeout) {
                std::cout << "\n\nTimeout: No messages received\n";
                break;
            }

            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - start
            );
            const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(timeout - elapsed).count();
            std::cout << "\r   Checking... " << remaining << " seconds remaining   " << std::flush;
            std::this_thread::sleep_for(poll_interval);
        }

        // =========================================
        // 5. Delete/forget email address
        // =========================================
        std::cout << "\nCleaning up email address...\n";
        try {
            const auto deleted = client.delete_email(email);
            if (deleted) {
                std::cout << "   Email address deleted\n";
            } else {
                std::cout << "   Deletion may have failed\n";
            }
        } catch (const guerrillamail::Error& error) {
            std::cerr << "   Error: " << error.what() << "\n";
        }

        std::cout << "\n" << std::string(50, '=') << "\n";
        std::cout << "Demo complete!\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Fatal error: " << error.what() << "\n";
        return 1;
    }
}
