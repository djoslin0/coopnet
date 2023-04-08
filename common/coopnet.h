#ifndef COOPNET_H
#define COOPNET_H
extern "C" {
#include <stdbool.h>

typedef enum {
    COOPNET_OK,
    COOPNET_FAILED,
    COOPNET_DISCONNECTED,
} CoopNetRc;

typedef struct {
    void (*OnDisconnected)(void);
} CoopNetCallbacks;

extern CoopNetCallbacks gCoopNetCallbacks;

CoopNetRc coopnet_begin(const char* aHost, uint32_t aPort);
CoopNetRc coopnet_shutdown(void);
CoopNetRc coopnet_update(void);
CoopNetRc coopnet_lobby_create(const char* aGame, const char* aVersion, const char* aTitle, uint16_t aMaxConnections);
CoopNetRc coopnet_lobby_join(uint64_t aLobbyId);
CoopNetRc coopnet_lobby_leave(uint64_t aLobbyId);
CoopNetRc coopnet_lobby_list_get(const char* aGame);
CoopNetRc coopnet_send(const uint8_t* aData, size_t aDataLength);
CoopNetRc coopnet_send_to(uint64_t aPeerId, const uint8_t* aData, size_t aDataLength);

}
#endif