#include <cstdint>
#include <string>
#include <thread>
#include <set>
#include <unistd.h>
#include <fcntl.h>

#include "connection.hpp"
#include "coopnet.h"
#include "logging.hpp"
#include "mpacket.hpp"

std::set<Connection*> sAllConnections;
std::mutex sAllConnectionsMutex;

Connection::Connection(uint64_t id) {
    mId = id;

    std::lock_guard<std::mutex> guard(sAllConnectionsMutex);
    sAllConnections.insert(this);
    LOG_INFO("Connections (added): %u", (uint32_t)sAllConnections.size());
}

Connection::~Connection() {
    std::lock_guard<std::mutex> guard(sAllConnectionsMutex);
    sAllConnections.erase(this);
    LOG_INFO("Connections (removed): %u", (uint32_t)sAllConnections.size());
}

bool Connection::IsValid(Connection* connection) {
    return (sAllConnections.count(connection) > 0);
}

void Connection::Begin() {
    // store info
    mActive = true;

    // set socket to non-blocking mode
    SocketSetNonBlocking(mSocket);

    // convert address
    char asciiAddress[256] = { 0 };
    inet_ntop(AF_INET, &(mAddress.sin_addr), asciiAddress, sizeof(asciiAddress));
    mAddressStr = asciiAddress;

    LOG_INFO("[%llu] Connection accepted: %s", mId, mAddressStr.c_str());
}

void Connection::Disconnect() {
    if (!mActive) { return; }

    if (mLobby) {
        mLobby->Leave(this);
    }

    mActive = false;
    SocketClose(mSocket);

    if (gCoopNetCallbacks.OnDisconnected) {
        gCoopNetCallbacks.OnDisconnected();
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
        //LOG_INFO("[%llu] continue", mId);
        return;
    } else if (ret == 0 || (rc == SOCKET_ECONNRESET)) {
        LOG_INFO("[%llu] Connection closed (%d, %d).", mId, ret, rc);
        Disconnect();
        return;
    } else if (ret < 0) {
        LOG_ERROR("[%llu] Error receiving data (%d)!", mId, rc);
        Disconnect();
        return;
    }

    /*LOG_INFO("[%llu] Received data:", mId);
    for (size_t i = 0; i < (size_t)ret; i++) {
        printf("  %02X", data[i]);
    }
    printf("\n");*/

    mDataSize += ret;
    MPacket::Read(this, mData, &mDataSize, MPACKET_MAX_SIZE);
}
