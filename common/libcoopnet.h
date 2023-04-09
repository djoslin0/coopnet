#ifndef LIBCOOPNET_H
#define LIBCOOPNET_H

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#else
#define EXPORT __attribute__((visibility("default")))
#endif

#if defined(__cplusplus) 
#include <cstdint>
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    COOPNET_OK,
    COOPNET_FAILED,
    COOPNET_DISCONNECTED,
} CoopNetRc;

enum MPacketErrorNumber {
    MERR_NONE,
    MERR_LOBBY_NOT_FOUND,
    MERR_LOBBY_JOIN_FULL,
    MERR_LOBBY_JOIN_FAILED,
    MERR_MAX,
};

typedef struct {
    void (*OnConnected)(uint64_t aUserId);
    void (*OnDisconnected)(void);
    void (*OnLobbyCreated)(uint64_t aLobbyId, const char* aGame, const char* aVersion, const char* aTitle, uint16_t aMaxConnections);
    void (*OnLobbyJoined)(uint64_t aLobbyId, uint64_t aUserId);
    void (*OnLobbyLeft)(uint64_t aLobbyId, uint64_t aUserId);
    void (*OnLobbyListGot)(uint64_t aLobbyId, uint64_t aOwnerId, uint16_t aConnections, uint16_t aMaxConnections, const char* aGame, const char* aVersion, const char* aTitle);
    void (*OnReceive)(uint64_t aFromUserId, const uint8_t* aData, uint64_t aSize);
    void (*OnError)(enum MPacketErrorNumber aErrorNumber);
    void (*OnPeerConnected)(uint64_t aPeerId);
    void (*OnPeerDisconnected)(uint64_t aPeerId);
} CoopNetCallbacks;

typedef struct {
    bool SkipWinsockInit;
} CoopNetSettings;

extern CoopNetCallbacks gCoopNetCallbacks;
extern CoopNetSettings gCoopNetSettings;

EXPORT bool coopnet_is_connected(void);
EXPORT CoopNetRc coopnet_begin(const char* aHost, uint32_t aPort);
EXPORT CoopNetRc coopnet_shutdown(void);
EXPORT CoopNetRc coopnet_update(void);
EXPORT CoopNetRc coopnet_lobby_create(const char* aGame, const char* aVersion, const char* aTitle, uint16_t aMaxConnections);
EXPORT CoopNetRc coopnet_lobby_join(uint64_t aLobbyId);
EXPORT CoopNetRc coopnet_lobby_leave(uint64_t aLobbyId);
EXPORT CoopNetRc coopnet_lobby_list_get(const char* aGame);
EXPORT CoopNetRc coopnet_send(const uint8_t* aData, uint64_t aDataLength);
EXPORT CoopNetRc coopnet_send_to(uint64_t aPeerId, const uint8_t* aData, uint64_t aDataLength);
EXPORT CoopNetRc coopnet_unpeer(uint64_t aPeerId);

#if defined(__cplusplus) 
}
#endif
#endif