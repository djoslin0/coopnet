#include <cstdint>
#include <string>
#include <thread>
#include <set>

#include "connection.hpp"
#include "logging.hpp"
#include "mpacket.hpp"

// callbacks
void (*gOnConnectionDisconnected)(Connection* connection) = nullptr;

std::set<Connection*> sAllConnections;
std::mutex sAllConnectionsMutex;

static void ReceiveStart(Connection* connection) {
    connection->Receive();
}

Connection::Connection(uint64_t id) {
    mId = id;

    std::lock_guard<std::mutex> guard(sAllConnectionsMutex);
    sAllConnections.insert(this);
}

Connection::~Connection() {
    std::lock_guard<std::mutex> guard(sAllConnectionsMutex);
    sAllConnections.erase(this);
}

bool Connection::IsValid(Connection* connection) {
    return (sAllConnections.count(connection) > 0);
}

void Connection::Begin() {
    // store info
    mActive = true;

    // set timeout
    struct timeval tv = { 10, 0 };
    setsockopt(mSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

    // convert address
    char asciiAddress[256] = { 0 };
    inet_ntop(AF_INET, &(mAddress.sin_addr), asciiAddress, sizeof(asciiAddress));
    mAddressStr = asciiAddress;

    LOG_INFO("[%lu] Connection accepted: %s", mId, mAddressStr.c_str());

    // create thread
    mThread = std::thread(ReceiveStart, this);
    mThread.detach();
}

void Connection::Receive() {
    uint8_t data[MPACKET_MAX_SIZE] = { 0 };
    uint16_t dataSize = 0;

    LOG_INFO("[%lu] Thread started", mId);

    while (true) {
        // receive from socket
        socklen_t len = sizeof(struct sockaddr_in);
        int ret = recvfrom(mSocket, &data[dataSize], MPACKET_MAX_SIZE - dataSize, 0, (struct sockaddr *) &mAddress, &len);
        int rc = errno;

        // make sure connection is still active
        if (!mActive) { break; }

        // check for error
        if (ret == -1 && (rc == EAGAIN || rc == EWOULDBLOCK)) {
            //LOG_INFO("[%lu] continue", mId);
            continue;
        } else if (ret == 0 || (rc ==  ECONNRESET)) {
            LOG_INFO("[%lu] Connection closed (%d, %d).", mId, ret, rc);
            break;
        } else if (ret < 0) {
            LOG_ERROR("[%lu] Error receiving data (%d)!", mId, rc);
            break;
        }

        /*LOG_INFO("[%lu] Received data:", mId);
        for (size_t i = 0; i < (size_t)ret; i++) {
            printf("  %02X", data[i]);
        }
        printf("\n");*/

        dataSize += ret;
        MPacket::Read(this, data, &dataSize, MPACKET_MAX_SIZE);
    }

    // inactivate
    mActive = false;

    // run callback
    if (gOnConnectionDisconnected) {
        gOnConnectionDisconnected(this);
    }
}
