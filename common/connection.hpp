#pragma once

#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "lobby.hpp"

class Lobby;

class Connection {
    private:
        std::string mAddressStr;
        std::thread mThread;

    public:
        bool mActive = false;
        uint64_t mId = 0;
        int mSocket = 0;
        struct sockaddr_in mAddress = { 0};
        Lobby* mLobby = nullptr;

        Connection(uint64_t id);
        ~Connection();
        static bool IsValid(Connection* connection);
        void Begin();
        void Receive();
};

// callbacks
extern void (*gOnConnectionDisconnected)(Connection* connection);
