#pragma once

#include <thread>
#include <vector>
#include <mutex>
#include "socket.hpp"
#include "connection.hpp"
#include "utils.hpp"

class Connection;

class Lobby {
    private:
    public:
        bool mActive = false;
        Connection* mOwner = nullptr;
        uint64_t mId = 0;
        std::vector<Connection*> mConnections;
        uint16_t mMaxConnections = 16;
        uint32_t mNextPriority = 0;

        std::string mGame;
        std::string mVersion;
        std::string mTitle;

        Lobby(Connection* aOwner, uint64_t aId, std::string& aGame, std::string& aVersion, std::string& aTitle, uint16_t aMaxConnections);
        ~Lobby();

        enum MPacketErrorNumber Join(Connection* aConnection);
        void Leave(Connection* aConnection);
};

// callbacks
extern void (*gOnLobbyJoin)(Lobby* lobby, Connection* connection);
extern void (*gOnLobbyLeave)(Lobby* lobby, Connection* connection);
extern void (*gOnLobbyDestroy)(Lobby* lobby);
