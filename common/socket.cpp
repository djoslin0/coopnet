#include "socket.hpp"
#include "libcoopnet.h"

#ifdef _WIN32
#include "logging.hpp"

int SocketInitialize(int aAf, int aType, int aProtocol) {
    // start up winsock
    if (!gCoopNetSettings.SkipWinsockInit) {
        WSADATA wsaData;
        int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (rc != NO_ERROR) {
            LOG_ERROR("WSAStartup failed with error %d", rc);
            return -1;
        }
    }

    //
    int sock = socket(aAf, aType, aProtocol);
    if (sock == -1) {
        LOG_ERROR("socket creation failed with error %d", SOCKET_LAST_ERROR);
        return sock;
    }

    int on = 1;
    setsockopt(sock, SOL_SOCKET, SO_DONTLINGER, (char*)&on, sizeof(on));

    return sock;
}

int SocketClose(int aSocket) {
    int ret = closesocket(aSocket);
    //WSACleanup();
    return ret;
}

void SocketSetNonBlocking(int aSocket) {
    // set socket to non-blocking mode
    u_long mode = 1;
    int rc = ioctlsocket(aSocket, FIONBIO, &mode);
    if (rc != 0) {
        LOG_ERROR("ioctlsocket failed with error: %d, %d", rc, SOCKET_LAST_ERROR);
        return INVALID_SOCKET;
    }
}

void SocketLimitBuffer(int aSocket, int64_t* amount) {
    unsigned long bufferLength = 0;
    ioctlsocket(aSocket, FIONREAD, &bufferLength);
    if (*amount > bufferLength) {
        *amount = bufferLength;
    }
}

#else

#include <sys/ioctl.h>

void SocketLimitBuffer(int aSocket, int64_t* amount) {
    int bufferLength = 0;
    ioctl(aSocket, FIONREAD, &bufferLength);
    if (*amount > bufferLength) {
        *amount = bufferLength;
    }
}

int SocketInitialize(int aAf, int aType, int aProtocol) {
    return socket(aAf, aType, aProtocol);
}

int SocketClose(int aSocket) {
    return close(aSocket);
}

void SocketSetNonBlocking(int aSocket) {
    // set socket to non-blocking mode
    int flags = fcntl(aSocket, F_GETFL, 0);
    fcntl(aSocket, F_SETFL, ((unsigned int)flags) | O_NONBLOCK);
}

#endif