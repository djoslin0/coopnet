#include <cstdint>
#include <string>
#include <thread>
#include <algorithm>

#include "lobby.hpp"
#include "logging.hpp"
#include "mpacket.hpp"

// callbacks
void (*gOnLobbyJoin)(Lobby* lobby, Connection* connection) = nullptr;
void (*gOnLobbyLeave)(Lobby* lobby, Connection* connection) = nullptr;
void (*gOnLobbyDestroy)(Lobby* lobby) = nullptr;

Lobby::Lobby(Connection* aOwner, uint64_t aId, std::string& aGame, std::string& aVersion, std::string& aTitle, uint16_t aMaxConnections) {
    mOwner = aOwner;
    mId = aId;
    mGame = aGame;
    mVersion = aVersion;
    mTitle = aTitle;
    mMaxConnections = aMaxConnections;
}

Lobby::~Lobby() {
    LOG_INFO("Destroying lobby %lu", mId);

    for (auto& it : mConnections) {
        this->Leave(it);
    }

    if (gOnLobbyDestroy) { gOnLobbyDestroy(this); }
}

bool Lobby::Join(Connection* aConnection) {
    // sanity check
    if (!aConnection) { return false; }
    if (aConnection->mLobby == this) { return false; }

    // leave older lobby
    if (aConnection->mLobby != nullptr) {
        aConnection->mLobby->Leave(aConnection);
    }

    // find out if we're already in this
    if (mConnections.size() >= mMaxConnections) {
        return false;
    }

    auto it = std::find(mConnections.begin(), mConnections.end(), aConnection);
    if (it == mConnections.end()) {
        mConnections.push_back(aConnection);
    }
    aConnection->mLobby = this;

    if (gOnLobbyJoin) { gOnLobbyJoin(this, aConnection); }
    return true;
}

void Lobby::Leave(Connection* aConnection) {
    if (!aConnection) { return; }
    if (aConnection->mLobby != this) { return; }

    if (gOnLobbyLeave) { gOnLobbyLeave(this, aConnection); }

    mConnections.erase(std::remove(mConnections.begin(), mConnections.end(), aConnection), mConnections.end());

    aConnection->mLobby = nullptr;

    if (mOwner == aConnection) {
        delete this;
    }
}