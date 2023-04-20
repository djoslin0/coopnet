#pragma once

#include "libcoopnet.h"
#include "socket.hpp"
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
        bool mIntentionalDisconnect = false;
        uint64_t mId = 0;
        uint64_t mDestinationId = 0;
        int mSocket = 0;
        struct sockaddr_in mAddress = { 0};
        Lobby* mLobby = nullptr;
        uint32_t mPriority = 0;

        Connection(uint64_t id);
        ~Connection();
        void Begin();
        void Disconnect(bool aIntentional);
        void Receive();
};
