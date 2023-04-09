#include <sys/types.h>
#include <chrono>

#include "socket.hpp"
#include "logging.hpp"
#include "server.hpp"
#include "connection.hpp"
#include "mpacket.hpp"
#include "utils.hpp"

Server* gServer = NULL;

StunTurnServer sStunServer = {
    .host = "stun.l.google.com",
	.username = "",
	.password = "",
    .port = 19302,
};

StunTurnServer sTurnServers[] = {
    {
        .host = "openrelay.metered.ca",
        .username = "openrelayproject",
        .password = "openrelayproject",
        .port = 80,
    },
};

static void sOnLobbyJoin(Lobby* lobby, Connection* connection) { gServer->OnLobbyJoin(lobby, connection); }
static void sOnLobbyLeave(Lobby* lobby, Connection* connection) { gServer->OnLobbyLeave(lobby, connection); }
static void sOnLobbyDestroy(Lobby* lobby) { gServer->OnLobbyDestroy(lobby); }

static void sReceiveStart(Server* server) { server->Receive(); }
static void sUpdateStart(Server* server)  { server->Update(); }

bool Server::Begin(uint32_t aPort) {

    // create a master socket
    mSocket = SocketInitialize(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (mSocket <= 0) {
        LOG_ERROR("Master socket failed (%d)!", mSocket);
        return false;
    }

    // set master socket to allow multiple connections ,
    // this is just a good habit, it will work without this
    socklen_t opt;
    if (setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt)) < 0) {
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
        // accept the incoming connection
        Connection* connection = new Connection(mNextConnectionId++);
        socklen_t len = sizeof(struct sockaddr_in);
        connection->mSocket = accept(mSocket, (struct sockaddr *) &connection->mAddress, &len);

        // make sure the connection worked
        if (connection->mSocket < 0) {
            LOG_ERROR("Failed to accept socket (%d)!", connection->mSocket);
            delete connection;
            continue;
        }

        // start connection
        connection->Begin();

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
        uint32_t turnServerCount = sizeof(sTurnServers) / sizeof(sTurnServers[0]);
        for (uint32_t i = 0; i < turnServerCount; i++) {
            StunTurnServer* turn = &sTurnServers[i];
            MPacketStunTurn(
                { .isStun = false, .port = turn->port },
                { turn->host, turn->username, turn->password }
            ).Send(*connection);
        }

        // remember connection
        std::lock_guard<std::mutex> guard(mConnectionsMutex);
        mConnections[connection->mId] = connection;
        LOG_INFO("[%llu] Connection added, count: %llu", connection->mId, mConnections.size());
    }
}

void Server::Update() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> guard(mConnectionsMutex);

        for (auto it = mConnections.begin(); it != mConnections.end(); ) {
            Connection* connection = it->second;
            // erase the connection if it's inactive, otherwise receive packets
            if (!connection->mActive) {
                it = mConnections.erase(it);
                LOG_INFO("[%llu] Connection removed, count: %llu", connection->mId, mConnections.size());
                delete connection;
            } else {
                it->second->Receive();
                ++it;
            }
        }
    }
}

Connection *Server::ConnectionGet(uint64_t aUserId) {
    return mConnections[aUserId];
}

Lobby* Server::LobbyGet(uint64_t aLobbyId) {
    return mLobbies[aLobbyId];
}

void Server::LobbyListGet(Connection& aConnection, std::string aGame) {
    for (auto& it : mLobbies) {
        if (!it.second) { continue; }
        if (it.second->mGame != aGame) { continue; }

        MPacketLobbyListGot({
            .lobbyId = it.first,
            .ownerId = it.second->mOwner->mId,
            .connections = (uint16_t)it.second->mConnections.size(),
            .maxConnections = it.second->mMaxConnections
        }, {
            it.second->mGame,
            it.second->mVersion,
            it.second->mTitle,
        }).Send(aConnection);
    }
}

void Server::OnLobbyJoin(Lobby* aLobby, Connection* aConnection) {
    MPacketLobbyJoined({
        .lobbyId = aLobby->mId,
        .userId = aConnection->mId,
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
    LOG_INFO("[%llu] Lobby removed, count: %llu", aLobby->mId, mLobbies.size());
}

void Server::LobbyCreate(Connection* aConnection, std::string& aGame, std::string& aVersion, std::string& aTitle, uint16_t aMaxConnections) {
    // check if this connection already has a lobby
    if (aConnection->mLobby) {
        aConnection->mLobby->Leave(aConnection);
    }

    // create the new lobby
    Lobby* lobby = new Lobby(aConnection, mNextLobbyId++, aGame, aVersion, aTitle, aMaxConnections);
    mLobbies[lobby->mId] = lobby;

    LOG_INFO("[%llu] Lobby added, count: %llu", lobby->mId, mLobbies.size());

    // notify of lobby creation
    MPacketLobbyCreated({
        .lobbyId = lobby->mId,
        .maxConnections = aMaxConnections,
    }, {
        lobby->mGame,
        lobby->mVersion,
        lobby->mTitle,
    }).Send(*aConnection);

    lobby->Join(aConnection);
}
