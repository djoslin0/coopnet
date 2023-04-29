#include <iostream>
#include "server.hpp"
#include "metrics.hpp"
#include "bansystem.hpp"

#define PORT 34197
#define EXIT_FAILURE 1

int main(int argc, char *argv[]) {
    Metrics metrics;
    ban_system_init();

    gServer = new Server();
    if (!gServer->Begin(PORT, ban_system_is_banned)) {
        exit(EXIT_FAILURE);
    }

    while (true) {
        metrics.Update(gServer->LobbyCount(), gServer->PlayerCount());
        ban_system_update();
        std::this_thread::sleep_for(std::chrono::milliseconds(60 * 1000));
    }
    return 0;
}