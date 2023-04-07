#include <thread>
#include <iostream>
#include <sstream>
#include <string>
#include "connection.hpp"
#include "mpacket.hpp"
#include "logging.hpp"

#define PORT 8888
#define EXIT_FAILURE 1

static Connection* sConnection = nullptr;
static std::thread sThreadUpdate;

static void sOnConnectionDisconnected(Connection* client) { exit(0); }

static void ClientUpdate() {
    while (sConnection->mActive) {
        MPacket::Process();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

int main(int argc, char const *argv[]) {
    sConnection = new Connection(0);

    // setup a socket
    sConnection->mSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(sConnection->mSocket == 0)
    {
        LOG_ERROR("Socket failed");
        exit(EXIT_FAILURE);
    }

    // type of socket created
    sConnection->mAddress.sin_family = AF_INET;
    sConnection->mAddress.sin_addr.s_addr = INADDR_ANY;
    sConnection->mAddress.sin_port = htons(PORT);

    // bind the socket to localhost port 8888
    if (connect(sConnection->mSocket, (struct sockaddr*) &sConnection->mAddress, sizeof(struct sockaddr_in)) < 0) {
        perror("connect failed");
        exit(EXIT_FAILURE);
    }

    // setup callbacks
    gOnConnectionDisconnected = sOnConnectionDisconnected;

    sConnection->Begin();

    // start thread
    sThreadUpdate = std::thread(ClientUpdate);
    sThreadUpdate.detach();

    while (sConnection->mActive) {
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
                MPacketLobbyCreate({
                    .maxConnections = (uint16_t)atoi(words[4].c_str())
                }, {
                    words[1],
                    words[2],
                    words[3]
                }).Send(*sConnection);
            } else {
                MPacketLobbyCreate({
                    .maxConnections = 16
                }, {
                    "sm64ex-coop",
                    "beta 999",
                    "Hello there, this is the text!"
                }).Send(*sConnection);
            }
        } else if (words[0] == "join" || words[0] == "j") {
            if (words.size() == 2) {
                MPacketLobbyJoin({
                    .lobbyId = (uint64_t)atoi(words[1].c_str())
                }).Send(*sConnection);
            }
        } else if (words[0] == "leave" || words[0] == "l") {
            if (words.size() == 2) {
                MPacketLobbyLeave({
                    .lobbyId = (uint64_t)atoi(words[1].c_str())
                }).Send(*sConnection);
            }
        } else if (words[0] == "list" || words[0] == "ls") {
            if (words.size() == 2) {
                MPacketLobbyListGet({
                    words[1]
                }).Send(*sConnection);
            } else {
                MPacketLobbyListGet({
                    "sm64ex-coop"
                }).Send(*sConnection);
            }
        }

    }
    return 0;
}