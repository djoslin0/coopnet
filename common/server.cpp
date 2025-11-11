#include <sys/types.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

#include "socket.hpp"
#include "logging.hpp"
#include "server.hpp"
#include "connection.hpp"
#include "mpacket.hpp"
#include "utils.hpp"

#define MAX_LOBBY_SIZE 16

Server* gServer = NULL;

StunTurnServer sStunServer = {
    .host = "stun.l.google.com",
    .username = "",
    .password = "",
    .port = 19302,
};

template <typename T>
const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

static void sOnLobbyJoin(Lobby* lobby, Connection* connection) { gServer->OnLobbyJoin(lobby, connection); }
static void sOnLobbyLeave(Lobby* lobby, Connection* connection) { gServer->OnLobbyLeave(lobby, connection); }
static void sOnLobbyDestroy(Lobby* lobby) { gServer->OnLobbyDestroy(lobby); }

static void sReceiveStart(Server* server) { server->Receive(); }
static void sUpdateStart(Server* server)  { server->Update(); }

void Server::ReadTurnServers() {
    mTurnServers.clear();
    std::ifstream input("turn-servers.cfg");
    std::string line;

    if (input.good()) {
        while (std::getline(input, line)) {
            std::size_t pos1 = line.find(":");
            std::size_t pos2 = line.find(":", pos1 + 1);
            std::size_t pos3 = line.find(":", pos2 + 1);
            if (pos1 == std::string::npos || pos2 == std::string::npos || pos3 == std::string::npos) { continue; }
            if (line.empty() || line[0] == '#') { continue; }

            StunTurnServer turn = {
                .host = line.substr(0, pos1),
                .username = line.substr(pos1 + 1, pos2 - pos1 - 1),
                .password = line.substr(pos2 + 1, pos3 - pos2 - 1),
                .port = (uint16_t)std::stoul(line.substr(pos3 + 1)),
            };
            mTurnServers.push_back(turn);
            LOG_ERROR("Loaded turn server: %s:%u", turn.host.c_str(), turn.port);
        }
    } else {
        LOG_ERROR("turn-servers.cfg not found");
    }

    input.close();
}

bool Server::Begin(uint32_t aPort) {
    // read TURN servers
    ReadTurnServers();

    // Use a PRNG to generate a random seed
    mPrng1 = std::mt19937_64(std::chrono::steady_clock::now().time_since_epoch().count() + 100);
    mPrng2 = std::mt19937_64(std::chrono::steady_clock::now().time_since_epoch().count() + 500);

    // create a master socket
    mSocket = SocketInitialize(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mSocket <= 0) {
        LOG_ERROR("Master socket failed (%d)!", mSocket);
        return false;
    }

    // set master socket to allow multiple connections ,
    // this is just a good habit, it will work without this
    socklen_t opt = { 0 };
    if (setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) != 0) {
        LOG_ERROR("Master socket failed to setsockopt!");
        return false;
    }

    // type of socket created
    struct sockaddr_in address = { 0 };
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(aPort);

    // bind the socket to localhost port 8888
    if (bind(mSocket, (struct sockaddr*) &address, sizeof(address)) < 0) {
        LOG_ERROR("Bind failed!");
        return false;
    }

    LOG_INFO("Listener on port %d", aPort);

    // try to specify maximum of 5 pending connections for the master socket
    if (listen(mSocket, 5) < 0) {
        LOG_ERROR("Master socket failed to listen!");
        return false;
    }

    // create threads
    mThreadRecv = std::thread(sReceiveStart, this);
    mThreadRecv.detach();
    mThreadUpdate = std::thread(sUpdateStart, this);
    mThreadUpdate.detach();

    // setup callbacks
    gOnLobbyJoin = sOnLobbyJoin;
    gOnLobbyLeave = sOnLobbyLeave;
    gOnLobbyDestroy = sOnLobbyDestroy;

    return true;
}

void Server::Receive() {
    LOG_INFO("Waiting for connections...");

    while (true) {
        // Get random connection id
        uint64_t connectionId = mRng(mPrng1);
        while (connectionId == 0 || mConnections.count(connectionId) > 0) {
            connectionId = mRng(mPrng1);
        }

        // accept the incoming connection
        Connection* connection = new Connection(connectionId);
        socklen_t len = sizeof(struct sockaddr_in);
        connection->mSocket = accept(mSocket, (struct sockaddr *) &connection->mAddress, &len);

        // make sure the connection worked
        if (connection->mSocket < 0) {
            LOG_ERROR("Failed to accept socket (%d)!", connection->mSocket);
            delete connection;
            continue;
        }

        // start connection
        connection->Begin(gCoopNetCallbacks.DestIdFunction);

        // check if connection is allowed
        if (gCoopNetCallbacks.ConnectionIsAllowed && !gCoopNetCallbacks.ConnectionIsAllowed(connection, true)) {
            QueueDisconnect(connection->mId, true);
        } else {
            // send join packet
            MPacketJoined({
                .userId = connection->mId,
                .version = MPACKET_PROTOCOL_VERSION
            }).Send(*connection);

            // send stun server
            MPacketStunTurn(
                { .isStun = true, .port = sStunServer.port },
                { sStunServer.host, sStunServer.username, sStunServer.password }
            ).Send(*connection);

            // send turn servers
            std::shuffle(mTurnServers.begin(), mTurnServers.end(), mPrng1);
            for (auto& it : mTurnServers) {
                MPacketStunTurn(
                    { .isStun = false, .port = it.port },
                    { it.host, it.username, it.password }
                ).Send(*connection);
            }
        }
        // remember connection
        std::lock_guard<std::mutex> guard(mConnectionsMutex);
        mConnections[connection->mId] = connection;
        LOG_INFO("[%" PRIu64 "] Connection added, count: %" PRIu64 "", connection->mId, (uint64_t)mConnections.size());
    }
}

void Server::Update() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> guard(mConnectionsMutex);
        int players = 0;
        size_t queueDisconnectCount = mQueueDisconnects.size();
        for (auto it = mConnections.begin(); it != mConnections.end(); ) {
            Connection* connection = it->second;
            // erase the connection if it's inactive, otherwise receive packets
            if (connection == nullptr) {
                it = mConnections.erase(it);
                continue;
            }

            if (!connection->mActive) {
                LOG_INFO("[%" PRIu64 "] Connection removed, count: %" PRIu64 "", connection->mId, (uint64_t)mConnections.size());
                delete connection;
                it = mConnections.erase(it);
                continue;
            } else {
                connection->Receive();
                connection->Update();
                if (connection->mLobby != nullptr) {
                    players++;
                }
            }

            std::chrono::system_clock::time_point nowTp = std::chrono::system_clock::now();
            uint64_t now = std::chrono::system_clock::to_time_t(nowTp);

            if (mRefreshBans && gCoopNetCallbacks.ConnectionIsAllowed && !gCoopNetCallbacks.ConnectionIsAllowed(connection, false)) {
                connection->Disconnect(true);
            } else if (mQueueDisconnects.count(connection->mId) > 0) {
                connection->Disconnect(true);
            } else if ((now - connection->mLastReceiveTime) > CONNECTION_DEAD_SECS) {
                LOG_INFO("[%" PRIu64 "] Connection timeout", connection->mId);
                connection->Disconnect(true);
            }

            ++it;
        }

        if (queueDisconnectCount == mQueueDisconnects.size()) {
            mQueueDisconnects.clear();
        }

        mRefreshBans = false;
        mPlayerCount = players;

        // clear null lobbies
        for (auto it = mLobbies.begin(); it != mLobbies.end(); ) {
            Lobby* lobby = it->second;
            if (!lobby) {
                it = mLobbies.erase(it);
                continue;
            }
            ++it;
        }

        ReputationUpdate();

        fflush(stdout);
        fflush(stderr);
    }
}

Connection *Server::ConnectionGet(uint64_t aUserId) {
    return mConnections[aUserId];
}

Lobby* Server::LobbyGet(uint64_t aLobbyId) {
    return mLobbies[aLobbyId];
}

void Server::LobbyListGet(Connection& aConnection, std::string aGame, std::string aPassword) {
    for (auto& it : mLobbies) {
        if (!it.second) { continue; }
        if (it.second->mGame != aGame) { continue; }
        if (it.second->mPassword != aPassword) { continue; }

        MPacketLobbyListGot({
            .lobbyId = it.first,
            .ownerId = it.second->mOwner->mId,
            .connections = (uint16_t)it.second->mConnections.size(),
            .maxConnections = it.second->mMaxConnections
        }, {
            it.second->mGame,
            it.second->mVersion,
            it.second->mHostName,
            it.second->mMode,
            it.second->mDescription,
        }).Send(aConnection);
    }
    MPacketLobbyListFinish({ 0 }).Send(aConnection);
}

void Server::OnLobbyJoin(Lobby* aLobby, Connection* aConnection) {
    if (!aLobby || !aConnection) { return; }
    if (gCoopNetCallbacks.LobbyConnectionIsAllowed && !gCoopNetCallbacks.LobbyConnectionIsAllowed(aConnection, aLobby)) { return; }
    MPacketLobbyJoined({
        .lobbyId = aLobby->mId,
        .userId = aConnection->mId,
        .ownerId = aLobby->mOwner->mId,
        .destId = aConnection->mDestinationId,
        .priority = aConnection->mPriority
    }).Send(*aLobby);

    // inform joiner of other connections
    for (auto& it : aLobby->mConnections) {
        if (it->mId == aConnection->mId) {
            continue;
        }
        MPacketLobbyJoined({
            .lobbyId = aLobby->mId,
            .userId = it->mId,
            .ownerId = aLobby->mOwner->mId,
            .destId = it->mDestinationId,
            .priority = it->mPriority
        }).Send(*aConnection);
    }
}

void Server::OnLobbyLeave(Lobby* aLobby, Connection* aConnection) {
    MPacketLobbyLeft({
        .lobbyId = aLobby->mId,
        .userId = aConnection->mId
    }).Send(*aLobby);
}

void Server::OnLobbyDestroy(Lobby* aLobby) {
    mLobbies.erase(aLobby->mId);
    mLobbyCount--;
    LOG_INFO("[%" PRIu64 "] Lobby removed, count: %" PRIu64 "", aLobby->mId, (uint64_t)mLobbies.size());
}

void Server::LobbyCreate(Connection* aConnection, std::string& aGame, std::string& aVersion, std::string& aHostName, std::string& aMode, uint16_t aMaxConnections, std::string& aPassword, std::string &aDescription) {
    // check if this connection already has a lobby
    if (aConnection->mLobby) {
        aConnection->mLobby->Leave(aConnection);
    }

    // Get random lobby id
    uint64_t lobbyId = mRng(mPrng2);
    while (lobbyId == 0 || mLobbies.count(lobbyId) > 0) {
        lobbyId = mRng(mPrng2);
    }

    // limit the lobby size
    if (aPassword == "" && aMaxConnections > MAX_LOBBY_SIZE) {
        aMaxConnections = MAX_LOBBY_SIZE;
    }

    // create the new lobby
    Lobby* lobby = new Lobby(aConnection,
        lobbyId,
        aGame,
        aVersion,
        aHostName,
        aMode,
        aMaxConnections,
        aPassword,
        aDescription);

    mLobbies[lobby->mId] = lobby;

    LOG_INFO("[%" PRIu64 "] Lobby added, count: %" PRIu64 "", lobby->mId, (uint64_t)mLobbies.size());

    // notify of lobby creation
    MPacketLobbyCreated({
        .lobbyId = lobby->mId,
        .maxConnections = aMaxConnections,
    }, {
        lobby->mGame,
        lobby->mVersion,
        lobby->mHostName,
        lobby->mMode
    }).Send(*aConnection);

    lobby->Join(aConnection, aPassword);
    mLobbyCount++;
}

void Server::LobbyUpdate(Connection *aConnection, uint64_t aLobbyId, std::string &aGame, std::string &aVersion, std::string &aHostName, std::string &aMode, std::string &aDescription) {
    Lobby* lobby = LobbyGet(aLobbyId);
    if (!lobby) {
        LOG_ERROR("Could not find lobby to update: %" PRIu64 "", aLobbyId);
        return;
    }
    if (lobby->mOwner != aConnection) {
        LOG_ERROR("Could not update lobby, was not the owner: %" PRIu64 "", aLobbyId);
        return;
    }

    lobby->mGame = aGame.substr(0, 32);
    lobby->mVersion = aVersion.substr(0, 32);
    lobby->mHostName = aHostName.substr(0, 32);
    lobby->mMode = aMode.substr(0, 32);
    lobby->mDescription = aDescription.substr(0, 256);
}

int Server::PlayerCount() {
    return mPlayerCount;
}

int Server::LobbyCount() {
    return mLobbyCount;
}

void Server::QueueDisconnect(uint64_t aUserId, bool aLockMutex) {
    if (aLockMutex) {
        std::lock_guard<std::mutex> guard(mConnectionsMutex);
        if (mQueueDisconnects.count(aUserId)) { return; }
        mQueueDisconnects.insert(aUserId);
    } else {
        if (mQueueDisconnects.count(aUserId)) { return; }
        mQueueDisconnects.insert(aUserId);
    }
}

void Server::RefreshBans() {
    std::lock_guard<std::mutex> guard(mConnectionsMutex);
    mRefreshBans = true;
}

void Server::ReputationIncrease(uint64_t aDestinationId) {
    std::chrono::system_clock::time_point nowTp = std::chrono::system_clock::now();
    uint64_t now = std::chrono::system_clock::to_time_t(nowTp);

    if (mReputation.count(aDestinationId) == 0) {
        mReputation[aDestinationId] = { 1, now };
    } else {
        mReputation[aDestinationId].value = clamp(mReputation[aDestinationId].value + 1, -16, 16);
        mReputation[aDestinationId].timestamp = now;
    }
    LOG_INFO("Reputation increase: destId %" PRIu64 " -> %d", aDestinationId, mReputation[aDestinationId].value);
}

void Server::ReputationDecrease(uint64_t aDestinationId) {
    std::chrono::system_clock::time_point nowTp = std::chrono::system_clock::now();
    uint64_t now = std::chrono::system_clock::to_time_t(nowTp);

    if (mReputation.count(aDestinationId) == 0) {
        mReputation[aDestinationId] = { -2, now };
    } else {
        mReputation[aDestinationId].value = clamp(mReputation[aDestinationId].value - 2, -16, 16);
        mReputation[aDestinationId].timestamp = now;
    }
    LOG_INFO("Reputation decrease: destId %" PRIu64 " -> %d", aDestinationId, mReputation[aDestinationId].value);
}

int32_t Server::ReputationGet(uint64_t aDestinationId) {
    if (mReputation.count(aDestinationId) == 0) {
        return 0;
    } else {
        return mReputation[aDestinationId].value;
    }
}

void Server::ReputationUpdate() {
    static uint64_t sNextRepUpdateTime = 0;
    std::chrono::system_clock::time_point nowTp = std::chrono::system_clock::now();
    uint64_t now = std::chrono::system_clock::to_time_t(nowTp);

    if (now < sNextRepUpdateTime) { return; }
    sNextRepUpdateTime = now + 60 * 60 * 1;

    for (auto it = mReputation.begin(); it != mReputation.end(); ) {
        uint64_t timestamp = it->second.timestamp;
        if ((now - timestamp) > 60 * 60 * 24) {
            it = mReputation.erase(it);
            continue;
        }
        ++it;
    }
}
