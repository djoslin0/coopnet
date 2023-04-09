#include "socket.hpp"
#include "logging.hpp"

#ifdef _WIN32

int SocketInitialize(int aAf, int aType, int aProtocol) {
    // start up winsock
    WSADATA wsaData;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc != NO_ERROR) {
        LOG_ERROR("WSAStartup failed with error %d", rc);
        return -1;
    }
    return socket(aAf, aType, aProtocol);
}

int SocketClose(int aSocket) {
    int ret = closesocket(aSocket);
    //WSACleanup();
    return ret;
}

void SocketSetNonBlocking(int aSocket) {
    // set socket to non-blocking mode
    u_long mode = 1;
    ioctlsocket(aSocket, FIONBIO, &mode);
}

#else

int SocketInitialize(int aAf, int aType, int aProtocol) {
    return socket(aAf, aType, aProtocol);
}

int SocketClose(int aSocket) {
    return close(aSocket);
}

void SocketSetNonBlocking(int aSocket) {
    // set socket to non-blocking mode
    int flags = fcntl(aSocket, F_GETFL, 0);
    fcntl(aSocket, F_SETFL, flags | O_NONBLOCK);
}

#endif