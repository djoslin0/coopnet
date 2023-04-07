#include <thread>
#include <iostream>
#include <sstream>
#include <string>
#include "client.hpp"
#include "connection.hpp"
#include "logging.hpp"

#define PORT 8888
#define EXIT_FAILURE 1

static void sOnConnectionDisconnected(Connection* connection) { exit(0); }

int main(int argc, char const *argv[]) {
    /*extern int test_connectivity();
    test_connectivity();
    exit(0);*/

    // setup callbacks
    gOnConnectionDisconnected = sOnConnectionDisconnected;

    gClient = new Client();

    if (!gClient->Begin(PORT)) {
        LOG_ERROR("Failed to begin client");
        exit(EXIT_FAILURE);
    }

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
                gClient->LobbyCreate(words[1], words[2], words[3], (uint16_t)atoi(words[4].c_str()));
            } else {
                gClient->LobbyCreate("sm64ex-coop", "beta 34", "This is a title!", 16);
            }
        } else if (words[0] == "join" || words[0] == "j") {
            if (words.size() == 2) {
                gClient->LobbyJoin((uint64_t)atoi(words[1].c_str()));
            }
        } else if (words[0] == "leave" || words[0] == "l") {
            if (words.size() == 2) {
                gClient->LobbyLeave((uint64_t)atoi(words[1].c_str()));
            }
        } else if (words[0] == "list" || words[0] == "ls") {
            if (words.size() == 2) {
                gClient->LobbyListGet(words[1]);
            } else {
                gClient->LobbyListGet("sm64ex-coop");
            }
        } else if (words[0] == "send" || words[0] == "s") {
            if (words.size() == 2) {
                gClient->PeerSend(words[1].c_str(), words[1].length() + 1);
            }
        }

    }
    return 0;
}