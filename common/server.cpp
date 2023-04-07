#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <chrono>

#include "logging.hpp"
#include "server.hpp"
#include "connection.hpp"
#include "mpacket.hpp"

Server* gServer = NULL;

static void sOnConnectionDisconnected(Connection* client) { gServer->OnClientDisconnect(client); }
static void sOnLobbyJoin(Lobby* lobby, Connection* client) { gServer->OnLobbyJoin(lobby, client); }
static void sOnLobbyLeave(Lobby* lobby, Connection* client) { gServer->OnLobbyLeave(lobby, client); }
static void sOnLobbyDestroy(Lobby* lobby) { gServer->OnLobbyDestroy(lobby); }

static void sReceiveStart(Server* server) { server->Receive(); }
static void sUpdateStart(Server* server)  { server->Update(); }

bool Server::Begin(uint32_t aPort) {
    // create a master socket
    mSocket = socket(AF_INET, SOCK_STREAM, 0);
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
    gOnConnectionDisconnected = sOnConnectionDisconnected;
    gOnLobbyJoin = sOnLobbyJoin;
    gOnLobbyLeave = sOnLobbyLeave;
    gOnLobbyDestroy = sOnLobbyDestroy;

    return true;
}

void Server::Receive() {
    LOG_INFO("Waiting for connections...");

    while (true) {
        // accept the incoming connection
        Connection* client = new Connection(mNextClientId++);
        socklen_t len = sizeof(struct sockaddr_in);
        client->mSocket = accept(mSocket, (struct sockaddr *) &client->mAddress, &len);

        // make sure the connection worked
        if (client->mSocket < 0) {
            LOG_ERROR("Failed to accept socket (%d)!", client->mSocket);
            delete client;
            continue;
        }

        // start client
        client->Begin();

        // send join packet
        MPacketJoined({
            .userId = client->mId,
            .version = MPACKET_PROTOCOL_VERSION
        }).Send(*client);

        // remember client
        std::lock_guard<std::mutex> guard(mClientsMutex);
        mClients[client->mId] = client;
        LOG_INFO("[%lu] Client added, count: %lu", client->mId, mClients.size());
    }
}

void Server::Update() {
    while (true) {
        MPacket::Process();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

Lobby* Server::LobbyGet(uint64_t aLobbyId) {
    return mLobbies[aLobbyId];
}

void Server::LobbyListGet(Connection& aClient, std::string aGame) {
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
        }).Send(aClient);
    }
}

void Server::OnClientDisconnect(Connection* client) {
    if (client->mLobby) {
        client->mLobby->Leave(client);
    }
    std::lock_guard<std::mutex> guard(mClientsMutex);
    mClients.erase(client->mId);
    LOG_INFO("[%lu] Client removed, count: %lu", client->mId, mClients.size());
    delete client;
}

void Server::OnLobbyJoin(Lobby* aLobby, Connection* aClient) {
    MPacketLobbyJoined({
        .lobbyId = aLobby->mId,
        .userId = aClient->mId
    }).Send(*aLobby);
}

void Server::OnLobbyLeave(Lobby* aLobby, Connection* aClient) {
    MPacketLobbyLeft({
        .lobbyId = aLobby->mId,
        .userId = aClient->mId
    }).Send(*aLobby);
}

void Server::OnLobbyDestroy(Lobby* aLobby) {
    mLobbies.erase(aLobby->mId);
    LOG_INFO("[%lu] Lobby removed, count: %lu", aLobby->mId, mLobbies.size());
}

void Server::LobbyCreate(Connection* aClient, std::string& aGame, std::string& aVersion, std::string& aTitle, uint16_t aMaxConnections) {
    // check if this client already has a lobby
    if (aClient->mLobby) {
        aClient->mLobby->Leave(aClient);
    }

    // create the new lobby
    Lobby* lobby = new Lobby(aClient, mNextLobbyId++, aGame, aVersion, aTitle, aMaxConnections);
    mLobbies[lobby->mId] = lobby;

    LOG_INFO("[%lu] Lobby added, count: %lu", lobby->mId, mLobbies.size());

    // notify of lobby creation
    MPacketLobbyCreated({
        .lobbyId = lobby->mId
    }, {
        lobby->mGame,
        lobby->mVersion,
        lobby->mTitle,
    }).Send(*aClient);

    lobby->Join(aClient);
}
