#include <iostream>
#include "server.hpp"
#include "metrics.hpp"

#define PORT 34197
#define EXIT_FAILURE 1

int main(int argc, char *argv[]) {
    Metrics metrics;
    gServer = new Server();
    if (!gServer->Begin(PORT)) {
        exit(EXIT_FAILURE);
    }

    while (true) {
        metrics.Update(gServer->LobbyCount(), gServer->ConnectionCount());
        std::this_thread::sleep_for(std::chrono::milliseconds(60 * 1000));
    }
    return 0;
}