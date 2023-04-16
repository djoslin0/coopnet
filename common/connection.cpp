#include <cstdint>
#include <string>
#include <set>
#include <functional>
#include <unistd.h>
#include <fcntl.h>

#include "connection.hpp"
#include "libcoopnet.h"
#include "logging.hpp"
#include "mpacket.hpp"

std::set<Connection*> sAllConnections;
std::mutex sAllConnectionsMutex;

Connection::Connection(uint64_t id) {
    mId = id;

    std::lock_guard<std::mutex> guard(sAllConnectionsMutex);
    sAllConnections.insert(this);
    LOG_INFO("Connections (added): %" PRIu64 "", (uint64_t)sAllConnections.size());
}

Connection::~Connection() {
    std::lock_guard<std::mutex> guard(sAllConnectionsMutex);
    sAllConnections.erase(this);
    LOG_INFO("Connections (removed): %" PRIu64 "", (uint64_t)sAllConnections.size());
}

bool Connection::IsValid(Connection* connection) {
    return (sAllConnections.count(connection) > 0);
}

void Connection::Begin() {
    // store info
    mActive = true;

    // get destination id
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getpeername(mSocket, (struct sockaddr*)&addr, &len);
    mDestinationId = (uint64_t)addr.sin_addr.s_addr;

    // set socket to non-blocking mode
    SocketSetNonBlocking(mSocket);

    // convert address
    char asciiAddress[256] = { 0 };
    inet_ntop(AF_INET, &(mAddress.sin_addr), asciiAddress, sizeof(asciiAddress));
    mAddressStr = asciiAddress;

    LOG_INFO("[%" PRIu64 "] Connection accepted: %s", mId, mAddressStr.c_str());
}

void Connection::Disconnect(bool aIntentional) {
    if (!mActive) { return; }

    if (mLobby) {
        mLobby->Leave(this);
    }

    mActive = false;
    SocketClose(mSocket);

    if (gCoopNetCallbacks.OnDisconnected) {
        gCoopNetCallbacks.OnDisconnected(aIntentional);
    }
}

void Connection::Receive() {
    // receive from socket
    socklen_t len = sizeof(struct sockaddr_in);
    int ret = recvfrom(mSocket, (char*)&mData[mDataSize], MPACKET_MAX_SIZE - mDataSize, MSG_DONTWAIT, (struct sockaddr *) &mAddress, &len);
    int rc = SOCKET_LAST_ERROR;

    // make sure connection is still active
    if (!mActive) { return; }

    // check for error
    if (ret == -1 && (rc == SOCKET_EAGAIN || rc == SOCKET_EWOULDBLOCK)) {
        //LOG_INFO("[%" PRIu64 "] continue", mId);
        return;
    } else if (ret == 0 || (rc == SOCKET_ECONNRESET)) {
        LOG_INFO("[%" PRIu64 "] Connection closed (%d, %d).", mId, ret, rc);
        Disconnect(false);
        return;
    } else if (ret < 0) {
        LOG_ERROR("[%" PRIu64 "] Error receiving data (%d)!", mId, rc);
        Disconnect(false);
        return;
    }

    /*LOG_INFO("[%" PRIu64 "] Received data:", mId);
    for (size_t i = 0; i < (size_t)ret; i++) {
        printf("  %02X", data[i]);
    }
    printf("\n");*/

    mDataSize += ret;
    MPacket::Read(this, mData, &mDataSize, MPACKET_MAX_SIZE);
}
