#include <iostream>

#include "guerrillamail/client.hpp"

int main() {
    guerrillamail::ClientOptions options;
    auto client = guerrillamail::Client::create(options);

    (void)client;

    std::cout << "guerrillamail-cpp skeleton example\n";
    return 0;
}
