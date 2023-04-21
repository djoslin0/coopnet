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

Connection::Connection(uint64_t id) {
    mId = id;
}

Connection::~Connection() {
}

void Connection::Begin() {
    // store info
    mActive = true;

    // get destination id
    struct sockaddr_in addr = { 0 };
    socklen_t len = sizeof(addr);
    getpeername(mSocket, (struct sockaddr*)&addr, &len);
    mDestinationId = (uint64_t)addr.sin_addr.s_addr;

    // set socket to non-blocking mode
    SocketSetNonBlocking(mSocket);
    int rc = SOCKET_LAST_ERROR;
    if (rc != 0) {
        LOG_ERROR("Socket non-blocking error: %d", rc);
    }


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
    // check buffer size
    int64_t remaining = (int64_t)MPACKET_MAX_SIZE - (int64_t)mDataSize;
    if (remaining <= 0) {
        LOG_ERROR("[%" PRIu64 "] Receive buffer full %" PRId64 "", mId, remaining);
        Disconnect(false);
        return;
    }
    if (remaining > 1024) { remaining = 1024; }

    // receive from socket
    socklen_t len = sizeof(struct sockaddr_in);
    int ret = recvfrom(mSocket, (char*)&mData[mDataSize], (size_t)remaining, MSG_DONTWAIT, (struct sockaddr *) &mAddress, &len);
    int rc = SOCKET_LAST_ERROR;
    //LOG_INFO("RECV: %d, %d, %" PRId64 ", %" PRId64, ret, rc, remaining, mDataSize);

    // make sure connection is still active
    if (!mActive) { return; }

    // check for error
    if (ret == 0 && rc == 0) {
        return;
    } else if ((ret == -1 || ret == 0) && (rc == SOCKET_EAGAIN || rc == SOCKET_EWOULDBLOCK)) {
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
