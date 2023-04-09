#pragma once
#include <string>
#include <arpa/inet.h>
#include <ctype.h>

typedef struct {
	std::string host;
	std::string username;
	std::string password;
	uint16_t port;
} StunTurnServer;

in_addr_t GetAddrFromDomain(const std::string& domain);
float clock_elapsed(void);
