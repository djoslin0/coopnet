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

static uint64_t sLastLobbyId = 0;

static void sOnDisconnected(bool aIntentional) { exit(0); }
static void sOnReceive(uint64_t aFromUserId, const uint8_t* aData, uint64_t aSize) {
    std::string msg((const char*)aData, (size_t)aSize);
    LOG_INFO("Received from %" PRIu64 ": %s", aFromUserId, msg.c_str());
}

static void sOnLobbyListGot(uint64_t aLobbyId, uint64_t aOwnerId, uint16_t aConnections, uint16_t aMaxConnections, const char* aGame, const char* aVersion, const char* aHostName, const char* aMode, const char* aDescription) {
    sLastLobbyId = aLobbyId;
}

static void sReceive(void) {
    while (coopnet_is_connected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        coopnet_update();
    }
}

static uint64_t stru64(const char* s) {
    char* end;
    return strtoull(s, &end, 10);
}

static void process_command(const std::vector<std::string>& words) {
    if (words[0] == "create" || words[0] == "c") {
        if (words.size() == 2) {
            coopnet_lobby_create("sm64coopdx2", "beta 34", "Host's Name", "Super Mario 64", 16, words[1].c_str(), "description!");
        } else if (words.size() == 6) {
            coopnet_lobby_create(words[1].c_str(), words[2].c_str(), words[3].c_str(), words[4].c_str(), (uint16_t)atoi(words[5].c_str()), "", "description!");
        } else if (words.size() == 7) {
            coopnet_lobby_create(words[1].c_str(), words[2].c_str(), words[3].c_str(), words[4].c_str(), (uint16_t)atoi(words[5].c_str()), words[6].c_str(), "description!");
        } else {
            coopnet_lobby_create("sm64coopdx2", "beta 34", "Host's Name", "Super Mario 64", 16, "", "description!");
        }
    } else if (words[0] == "join" || words[0] == "j") {
        if (words.size() == 1 && sLastLobbyId != 0) {
            coopnet_lobby_join(sLastLobbyId, "");
        } else if (words.size() == 2) {
            coopnet_lobby_join((uint64_t)stru64(words[1].c_str()), "");
        } else if (words.size() == 3) {
            coopnet_lobby_join((uint64_t)stru64(words[1].c_str()), words[2].c_str());
        }
    } else if (words[0] == "leave" || words[0] == "l") {
        if (words.size() == 2) {
            coopnet_lobby_leave((uint64_t)stru64(words[1].c_str()));
        }
    } else if (words[0] == "list" || words[0] == "ls") {
        if (words.size() == 3) {
            coopnet_lobby_list_get(words[1].c_str(), words[2].c_str());
        } else if (words.size() == 2) {
            coopnet_lobby_list_get("sm64coopdx2", words[1].c_str());
        } else {
            coopnet_lobby_list_get("sm64coopdx2", "");
        }
    } else if (words[0] == "send" || words[0] == "s") {
        if (words.size() == 2) {
            coopnet_send((const uint8_t*)words[1].c_str(), words[1].length() + 1);
        }
    } else if (words[0] == "connect") {
        gCoopNetCallbacks.OnDisconnected = sOnDisconnected;
        coopnet_begin(HOST, PORT, "example", 0);
        sThreadRecv = std::thread(sReceive);
        sThreadRecv.detach();
    } else if (words[0] == "disconnect") {
        gCoopNetCallbacks.OnDisconnected = nullptr;
        coopnet_shutdown();
    } else if (words[0] == "unpeer") {
        if (words.size() == 2) {
            coopnet_unpeer((uint64_t)stru64(words[1].c_str()));
        }
    }
}

int main(int argc, char const *argv[]) {
    // setup callbacks
    gCoopNetCallbacks.OnDisconnected = sOnDisconnected;
    gCoopNetCallbacks.OnReceive = sOnReceive;
    gCoopNetCallbacks.OnLobbyListGot = sOnLobbyListGot;

    if (coopnet_begin(HOST, PORT, "example", 999) != COOPNET_OK) {
        LOG_ERROR("Failed to begin client");
        exit(EXIT_FAILURE);
    }

    sThreadRecv = std::thread(sReceive);
    sThreadRecv.detach();

    std::vector<std::string> words;
    words.push_back("list");
    process_command(words);
    words.clear();

    if (argc > 1) {
        for(int i = 1; i < argc; ++i) {
            words.push_back(argv[i]);
        }
        if (words[0][0] == 'j') {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        process_command(words);
    }

    //coopnet_lobby_create("sm64coopdx2", "beta 34", "Host's Name", "Super Mario 64", 16, "", "description!");

    while (true) {
        std::string input;
        std::getline(std::cin, input);

        std::istringstream iss(input);
        std::vector<std::string> words;
        std::string word;

        while (std::getline(iss, word, ' ')) {
            words.push_back(word);
        }

        if (words.empty()) {
            continue;
        }

        process_command(words);

    }
    return 0;
}
