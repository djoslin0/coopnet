#include "client.hpp"
#include "mpacket.hpp"
#include "logging.hpp"

Client* gClient = NULL;

static void sUpdateStart(Client* client)  { client->Update(); }

bool Client::Begin(uint32_t aPort)
{
    mConnection = new Connection(0);

    // setup a socket
    mConnection->mSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(mConnection->mSocket == 0)
    {
        LOG_ERROR("Socket failed");
        return false;
    }

    // type of socket created
    mConnection->mAddress.sin_family = AF_INET;
    mConnection->mAddress.sin_addr.s_addr = INADDR_ANY;
    mConnection->mAddress.sin_port = htons(aPort);

    // bind the socket to localhost port 8888
    if (connect(mConnection->mSocket, (struct sockaddr*) &mConnection->mAddress, sizeof(struct sockaddr_in)) < 0) {
        LOG_ERROR("Connect failed");
        return false;
    }

    mConnection->Begin();

    // start thread
    mThreadUpdate = std::thread(sUpdateStart, this);
    mThreadUpdate.detach();

    return true;
}

void Client::Update() {
    while (mConnection->mActive) {
        MPacket::Process();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void Client::PeerBegin(uint64_t userId) {
    mPeers[userId] = new Peer(userId);
    LOG_INFO("Peer begin, count: %lu", mPeers.size());
}

void Client::PeerEnd(uint64_t userId) {
    Peer* peer = mPeers[userId];
    if (peer) {
        peer->Disconnect();
        delete peer;
    }
    mPeers.erase(userId);
    LOG_INFO("Peer end, count: %lu", mPeers.size());
}

void Client::PeerEndAll() {
    for (auto& it : mPeers) {
        Peer* peer = it.second;
        if (peer) {
            peer->Disconnect();
            delete peer;
        }
    }
    mPeers.clear();
    LOG_INFO("Peer end all, count: %lu", mPeers.size());
}

Peer* Client::PeerGet(uint64_t userId) {
    return mPeers[userId];
}
void Client::PeerSend(const char* aData, size_t aDataLength) {
    for (auto& it : mPeers) {
        Peer* peer = it.second;
        if (peer) {
            peer->Send(aData, aDataLength);
        }
    }
}

void Client::LobbyCreate(std::string aGame, std::string aVersion, std::string aTitle, uint16_t aMaxConnections) {
    MPacketLobbyCreate(
        { .maxConnections = aMaxConnections },
        { aGame, aVersion, aTitle }
        ).Send(*mConnection);
}

void Client::LobbyJoin(uint64_t aLobbyId) {
    MPacketLobbyJoin(
        { .lobbyId = aLobbyId }
        ).Send(*mConnection);
}

void Client::LobbyLeave(uint64_t aLobbyId) {
    MPacketLobbyLeave(
        { .lobbyId = aLobbyId }
        ).Send(*mConnection);
}

void Client::LobbyListGet(std::string aGame) {
    MPacketLobbyListGet(
        {},
        { aGame }
        ).Send(*mConnection);
}
