#include <iostream>
#include "server.hpp"

#define PORT 34197
#define EXIT_FAILURE 1

int main(int argc, char *argv[]) {
    gServer = new Server();
    if (!gServer->Begin(PORT)) {
        exit(EXIT_FAILURE);
    }

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    return 0;
}