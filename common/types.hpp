#pragma once

#include <string>
#include <ctype.h>

enum MPacketErrorNumber {
    MERR_NONE,
    MERR_LOBBY_NOT_FOUND,
    MERR_LOBBY_JOIN_FULL,
    MERR_LOBBY_JOIN_FAILED,
    MERR_MAX,
};

typedef struct {
	std::string host;
	std::string username;
	std::string password;
	uint16_t port;
} StunTurnServer;
