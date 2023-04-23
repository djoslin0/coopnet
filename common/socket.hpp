#pragma once

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>
#define in_addr_t ULONG
#define MSG_DONTWAIT 0
#define MSG_NOSIGNAL 0

#define SOCKET_RESET_ERROR() WSASetLastError(0)
#define SOCKET_LAST_ERROR WSAGetLastError()
#define SOCKET_EAGAIN EAGAIN
#define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCKET_ECONNRESET WSAECONNRESET

#else

#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

#define SOCKET_RESET_ERROR() errno = 0
#define SOCKET_LAST_ERROR errno
#define SOCKET_EAGAIN EAGAIN
#define SOCKET_EWOULDBLOCK EWOULDBLOCK
#define SOCKET_ECONNRESET ECONNRESET

#endif

int SocketInitialize(int aAf, int aType, int aProtocol);
int SocketClose(int aSocket);
void SocketSetNonBlocking(int aSocket);
void SocketLimitBuffer(int aSocket, int64_t* amount);
