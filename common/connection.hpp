#pragma once

#include "libcoopnet.h"
#include "socket.hpp"
#include "mpacket.hpp"
#include "lobby.hpp"
#include <map>

class Lobby;

#define CONNECTION_KEEP_ALIVE_SECS (60 * 3)
#define CONNECTION_DEAD_SECS (60 * 4)

class Connection {
    private:
        uint8_t mData[MPACKET_MAX_SIZE] = { 0 };
        int64_t mDataSize = 0;
        std::map<uint64_t, uint64_t> mPeerTimeouts;

    public:
        bool mActive = false;
        bool mIntentionalDisconnect = false;
        uint64_t mId = 0;
        uint64_t mDestinationId = 0;
        uint64_t mInfoBits = 0;
        bool mUpdated = false;
        int mSocket = 0;
        struct sockaddr_in mAddress = { 0};
        Lobby* mLobby = nullptr;
        uint32_t mPriority = 0;
        uint64_t mLastSendTime = 0;
        uint64_t mLastReceiveTime = 0;
        std::string mAddressStr;
        uint64_t mHash;

        Connection(uint64_t id);
        ~Connection();
        void Begin(uint64_t (*aDestIdFunction)(uint64_t aInput));
        void Disconnect(bool aIntentional);
        void Update();
        void Receive();

        void PeerBegin(uint64_t aPeerId);
        void PeerFail(uint64_t aPeerId);
};
