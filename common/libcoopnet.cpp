#include "client.hpp"
#include <stdbool.h>
#include "libcoopnet.h"

CoopNetCallbacks gCoopNetCallbacks = { 0 };
CoopNetSettings gCoopNetSettings = { 0 };

bool coopnet_is_connected(void) {
    return (gClient && gClient->mConnection && gClient->mConnection->mActive);
}

CoopNetRc coopnet_begin(const char* aHost, uint32_t aPort) {
    if (gClient) { return COOPNET_OK; }

    gClient = new Client();
    bool ret = gClient->Begin(aHost, aPort);
    
    if (!ret) {
        coopnet_shutdown();
    }

    return ret
        ? COOPNET_OK
        : COOPNET_FAILED;
}

CoopNetRc coopnet_shutdown(void) {
    if (!gClient) { return COOPNET_DISCONNECTED; }

    gClient->mShutdown = true;

    return COOPNET_OK;
}

CoopNetRc coopnet_update(void) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->Update();

    if (gClient->mShutdown) {
        delete gClient;
        gClient = nullptr;
    }
    return COOPNET_OK;
}

CoopNetRc coopnet_lobby_create(const char* aGame, const char* aVersion, const char* aTitle, uint16_t aMaxConnections) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->LobbyCreate(aGame, aVersion, aTitle, aMaxConnections);
    return COOPNET_OK;
}

CoopNetRc coopnet_lobby_join(uint64_t aLobbyId) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->LobbyJoin(aLobbyId);
    return COOPNET_OK;
}

CoopNetRc coopnet_lobby_leave(uint64_t aLobbyId) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->LobbyLeave(aLobbyId);
    return COOPNET_OK;
}

CoopNetRc coopnet_lobby_list_get(const char* aGame) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    gClient->LobbyListGet(aGame);
    return COOPNET_OK;
}

CoopNetRc coopnet_send(const uint8_t* aData, uint64_t aDataLength) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    return gClient->PeerSend(aData, aDataLength)
        ? COOPNET_OK
        : COOPNET_FAILED;
}

CoopNetRc coopnet_send_to(uint64_t aPeerId, const uint8_t* aData, uint64_t aDataLength) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    return gClient->PeerSendTo(aPeerId, aData, aDataLength)
        ? COOPNET_OK
        : COOPNET_FAILED;
}

CoopNetRc coopnet_unpeer(uint64_t aPeerId) {
    if (!gClient) { return COOPNET_DISCONNECTED; }
    Peer* peer = gClient->PeerGet(aPeerId);
    if (!peer) {
        return COOPNET_FAILED;
    }
    peer->Disconnect();
    return COOPNET_OK;
}
