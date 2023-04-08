#pragma once

#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "mpacket.hpp"
#include "lobby.hpp"

class Lobby;

class Connection {
    private:
        std::string mAddressStr;
        uint8_t mData[MPACKET_MAX_SIZE] = { 0 };
        uint16_t mDataSize = 0;

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
        void Disconnect();
        void Receive();
};
