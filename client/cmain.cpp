#include <thread>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "libcoopnet.h"
#include "logging.hpp"

#define HOST "localhost"
#define PORT 34197
#define EXIT_FAILURE 1

std::thread sThreadRecv;

static void sOnDisconnected(void) { exit(0); }

static void sReceive(void) {
    while (coopnet_is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        coopnet_update();
    }
}

int main(int argc, char const *argv[]) {
    // setup callbacks
    gCoopNetCallbacks.OnDisconnected = sOnDisconnected;

    if (coopnet_begin(HOST, PORT) != COOPNET_OK) {
        LOG_ERROR("Failed to begin client");
        exit(EXIT_FAILURE);
    }

    sThreadRecv = std::thread(sReceive);
    sThreadRecv.detach();

    while (true) {
        std::string input;
        std::getline(std::cin, input);

        std::istringstream iss(input);
        std::vector<std::string> words;
        std::string word;

        while (std::getline(iss, word, ' ')) {
            words.push_back(word);
        }

        if (words.size() == 0) {
            continue;
        }

        if (words[0] == "create" || words[0] == "c") {
            if (words.size() == 5) {
                coopnet_lobby_create(words[1].c_str(), words[2].c_str(), words[3].c_str(), (uint16_t)atoi(words[4].c_str()));
            } else {
                coopnet_lobby_create("sm64ex-coop", "beta 34", "This is a title!", 16);
            }
        } else if (words[0] == "join" || words[0] == "j") {
            if (words.size() == 2) {
                coopnet_lobby_join((uint64_t)atoi(words[1].c_str()));
            }
        } else if (words[0] == "leave" || words[0] == "l") {
            if (words.size() == 2) {
                coopnet_lobby_leave((uint64_t)atoi(words[1].c_str()));
            }
        } else if (words[0] == "list" || words[0] == "ls") {
            if (words.size() == 2) {
                coopnet_lobby_list_get(words[1].c_str());
            } else {
                coopnet_lobby_list_get("sm64ex-coop");
            }
        } else if (words[0] == "send" || words[0] == "s") {
            if (words.size() == 2) {
                coopnet_send((const uint8_t*)words[1].c_str(), words[1].length() + 1);
            }
        } else if (words[0] == "connect") {
            gCoopNetCallbacks.OnDisconnected = sOnDisconnected;
            coopnet_begin(HOST, PORT);
            sThreadRecv = std::thread(sReceive);
            sThreadRecv.detach();
        } else if (words[0] == "disconnect") {
            gCoopNetCallbacks.OnDisconnected = nullptr;
            coopnet_shutdown();
        } else if (words[0] == "unpeer") {
            if (words.size() == 2) {
                coopnet_unpeer((uint64_t)atoi(words[1].c_str()));
            }
        }

    }
    return 0;
}