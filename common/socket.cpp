#include "socket.hpp"
#include "libcoopnet.h"
#include "logging.hpp"

#ifdef _WIN32

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

    SOCKET_RESET_ERROR();
    int sock = socket(aAf, aType, aProtocol);
    if (sock == -1) {
        LOG_ERROR("socket creation failed with error %d", SOCKET_LAST_ERROR);
        return sock;
    }

    return sock;
}

int SocketClose(int aSocket) {
    int ret = closesocket(aSocket);
    //WSACleanup();
    return ret;
}

void SocketSetOptions(int aSocket) {
    // set socket to non-blocking mode
    SOCKET_RESET_ERROR();
    u_long mode = 1;
    if (ioctlsocket(aSocket, FIONBIO, &mode) != 0) {
        LOG_ERROR("failed to set non-blocking: %d", SOCKET_LAST_ERROR);
    }

    // set socket to keep-alive mode
    SOCKET_RESET_ERROR();
    int keepAlive = 1;
    int result = setsockopt(aSocket, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(keepAlive));
    if (result == SOCKET_ERROR) {
        LOG_ERROR("failed to set keep-alive: %d", SOCKET_LAST_ERROR);
    }

    // set socket to dont-linger
    SOCKET_RESET_ERROR();
    int on = 1;
    if (setsockopt(aSocket, SOL_SOCKET, SO_DONTLINGER, (char*)&on, sizeof(on)) < 0) {
        LOG_ERROR("failed to set dont-linger: %d", SOCKET_LAST_ERROR);
    }

}

void SocketLimitBuffer(int aSocket, int64_t* amount) {
    SOCKET_RESET_ERROR();
    unsigned long bufferLength = 0;
    if (ioctlsocket(aSocket, FIONREAD, &bufferLength) != 0) {
        LOG_ERROR("failed to retrieve buffer size: %d", SOCKET_LAST_ERROR);
        return;
    }
    if (*amount > bufferLength) {
        *amount = bufferLength;
    }
}

#else

#include <sys/ioctl.h>

int SocketInitialize(int aAf, int aType, int aProtocol) {
    return socket(aAf, aType, aProtocol);
}

int SocketClose(int aSocket) {
    return close(aSocket);
}

void SocketSetOptions(int aSocket) {
    // set socket to non-blocking mode
    int flags = fcntl(aSocket, F_GETFL, 0);
    SOCKET_RESET_ERROR();
    int rc = fcntl(aSocket, F_SETFL, ((unsigned int)flags) | O_NONBLOCK);
    if (rc == -1) {
        LOG_ERROR("failed to set to non-blocking: %d", SOCKET_LAST_ERROR);
    }

    // set socket to keep-alive mode
    SOCKET_RESET_ERROR();
    int optval = 1;
    if(setsockopt(aSocket, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
        LOG_ERROR("failed to set to keep-alive mode: %d", SOCKET_LAST_ERROR);
    }

    // set socket to dont-linger
    SOCKET_RESET_ERROR();
    struct linger lingerStruct = { 1, 0 };
    if (setsockopt(aSocket, SOL_SOCKET, SO_LINGER, &lingerStruct, sizeof(lingerStruct)) < 0) {
        LOG_ERROR("failed to set to dont-linger mode: %d", SOCKET_LAST_ERROR);
    }
}

void SocketLimitBuffer(int aSocket, int64_t* amount) {
    SOCKET_RESET_ERROR();
    int bufferLength = 0;
    if (ioctl(aSocket, FIONREAD, &bufferLength) == -1) {
        LOG_ERROR("failed to retrieve buffer size: %d", SOCKET_LAST_ERROR);
        return;
    }
    if (*amount > bufferLength) {
        *amount = bufferLength;
    }
}

#endif
