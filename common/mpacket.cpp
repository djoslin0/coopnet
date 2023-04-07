#include <cstdint>
#include <map>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include "mpacket.hpp"
#include "logging.hpp"
#include "server.hpp"

static MPacket* sPacketByType[MPACKET_MAX] = {
    new MPacket(),
    new MPacketJoined(),
    new MPacketLobbyCreate(),
    new MPacketLobbyCreated(),
    new MPacketLobbyJoin(),
    new MPacketLobbyJoined(),
    new MPacketLobbyLeave(),
    new MPacketLobbyLeft(),
    new MPacketLobbyListGet(),
    new MPacketLobbyListGot(),
};

struct MPacketProccess {
    Connection* connection;
    uint8_t* data;
};

std::vector<struct MPacketProccess> sPacketProcess;
std::mutex sPacketProcessMutex;

void MPacket::Send(Connection& connection) {
    // make sure its connected
    if (!connection.mActive) {
        return;
    }

    // figure out string size
    uint16_t stringSize = 0;
    for (const auto& s : mStringData) {
        stringSize += sizeof(uint16_t) + strlen(s.c_str());
    }

    // setup packet header
    MPacketImplSettings impl = GetImplSettings();
    MPacketHeader pHeader = {
        .packetType = impl.packetType,
        .dataSize = mVoidDataSize,
        .stringSize = stringSize
    };

    // figure out data size
    size_t dataSize = sizeof(MPacketHeader) + pHeader.dataSize + pHeader.stringSize;
    if (dataSize > MPACKET_MAX_SIZE) {
        LOG_ERROR("Packet size exceeded max size (%lu > %lu)", dataSize, MPACKET_MAX_SIZE);
        return;
    }

    // allocate data
    uint8_t* data = (uint8_t*) malloc(dataSize);
    if (!data) {
        LOG_ERROR("Error allocating data");
        return;
    }

    // fill header
    uint8_t* d = &data[0];
    memcpy(d, &pHeader, sizeof(MPacketHeader));
    d += sizeof(MPacketHeader);

    // fill data
    memcpy(d, mVoidData, mVoidDataSize);
    d += mVoidDataSize;

    // fill strings
    for (const auto& s : mStringData) {
        const char* c = s.c_str();

        // fill string length
        uint16_t slength = strlen(c);
        *((uint16_t*)d) = slength;
        d += sizeof(uint16_t);

        // fill string
        for (uint8_t i = 0; i < slength; i++) {
            *d = *c;
            d++;
            c++;
        }
    }

    // send data buffer
    size_t sent = sendto(connection.mSocket, &data[0], dataSize, 0, (const sockaddr*)&connection.mAddress, sizeof(struct sockaddr_in));
    int rc = errno;

    // debug print packet
    /*LOG_INFO("Sent data:");
    for (size_t i = 0; i < dataSize; i++) {
        printf("  %02X", data[i]);
    }
    printf("\n");*/

    // free data buffer
    free(data);

    // check for send error
    if (sent < 0) {
        LOG_ERROR("Error sending data (%d)!", rc);
    }

    // check for data size error
    if (sent != dataSize) {
        LOG_ERROR("Error sending data, did not send all bytes (%lu != %lu)!", sent, dataSize);
    }
}

void MPacket::Send(Lobby& lobby) {
    for (auto& it : lobby.mConnections) {
        Send(*it);
    }
}

void MPacket::Process() {
    std::lock_guard<std::mutex> guard(sPacketProcessMutex);
    for (auto& it : sPacketProcess) {
        Connection* connection = it.connection;
        if (!Connection::IsValid(connection)) { continue; }

        MPacketHeader header = *(MPacketHeader*)it.data;
        void* voidData = &it.data[sizeof(MPacketHeader)];
        void* stringData = &it.data[sizeof(MPacketHeader) + header.dataSize];
        bool parseError = false;

        // sanity check packet type
        if (header.packetType >= MPACKET_MAX || header.packetType == MPACKET_NONE) {
            LOG_ERROR("Received an unknown packet type: %u, data size: %u, string size: %u", header.packetType, header.dataSize, header.stringSize);
            continue;
        }

        // receive packet
        MPacket* packet = sPacketByType[header.packetType];

        // sanity check data size
        if (header.dataSize != packet->mVoidDataSize) {
            LOG_ERROR("Received the wrong data size: %u != %u", header.dataSize, packet->mVoidDataSize);
            continue;
        }

        // receive data
        memcpy(packet->mVoidData, voidData, packet->mVoidDataSize);

        // receive strings
        packet->mStringData.clear();
        uint8_t* c = (uint8_t*)stringData;
        uint8_t* climit = c + header.stringSize;
        while (c < climit) {
            // retrieve string length
            uint16_t length = *(uint16_t*)c;
            c += sizeof(uint16_t);
            if (c >= climit) { parseError = true; break; }

            // allocate string
            char* cstr = (char*)malloc(length + 1);
            if (!cstr) {
                LOG_ERROR("Failed to allocate string");
                parseError = true;
                break;
            }

            // fill string
            snprintf(cstr, length + 1, "%s", c);
            c += length;

            // remember string
            packet->mStringData.push_back(cstr);
            free(cstr);
        }
        if (c != climit) { parseError = true; }

        // check impl settings
        MPacketImplSettings impl = packet->GetImplSettings();
        if (header.packetType != impl.packetType) {
            LOG_ERROR("Received packet type mismatch: %u != %u", header.packetType, impl.packetType);
            continue;
        }
        if (packet->mStringData.size() != impl.stringCount) {
            LOG_ERROR("Received packet string count mismatch!");
            continue;
        }
        if (gServer && impl.serverPacket) {
            LOG_ERROR("Received server packet while being a server!");
            continue;
        }
        if (!gServer && !impl.serverPacket) {
            LOG_ERROR("Received client packet while being a client!");
            continue;
        }

        // receive the packet
        if (parseError) {
            LOG_ERROR("Packet parse error!");
        } else {
            bool ret = packet->Receive(connection);
            if (!ret) { LOG_ERROR("Packet receive error!"); }
        }
    }
    sPacketProcess.clear();

}

void MPacket::Read(Connection* connection, uint8_t* aData, uint16_t* aDataSize, uint16_t aMaxDataSize) {
    while (true) {
        MPacketHeader header = *(MPacketHeader*)aData;
        uint16_t totalSize = sizeof(MPacketHeader) + header.dataSize + header.stringSize;

        // check the received size
        if (*aDataSize < totalSize) {
            return;
        }

        // queue up the packet to be processed
        uint8_t* processData = (uint8_t*)malloc(totalSize);
        if (processData) {
            memcpy(processData, aData, totalSize);

            std::lock_guard<std::mutex> guard(sPacketProcessMutex);
            sPacketProcess.push_back({
                .connection = connection,
                .data = processData
            });

        } else {
            LOG_ERROR("Could not allocate packet data to process");
        }

        // shift the data array
        uint16_t j = 0;
        for (uint16_t i = totalSize; i < *aDataSize; i++) {
            aData[j++] = aData[i];
        }

        // reduce the current buffer size
        *aDataSize -= totalSize;
    }
}

bool MPacketJoined::Receive(Connection* connection) {
    LOG_INFO("MPACKET_JOINED received: userID %lu, version %u", mData.userId, mData.version);
    return true;
}

bool MPacketLobbyCreate::Receive(Connection* connection) {
    std::string& game = mStringData[0];
    std::string& version = mStringData[1];
    std::string& title = mStringData[2];

    LOG_INFO("MPACKET_LOBBY_CREATE received: game '%s', version '%s', title '%s', maxconnections %u", game.c_str(), version.c_str(), title.c_str(), mData.maxConnections);
    gServer->LobbyCreate(connection, game, version, title, mData.maxConnections);

    return true;
}

bool MPacketLobbyCreated::Receive(Connection* connection) {
    std::string& game = mStringData[0];
    std::string& version = mStringData[1];
    std::string& title = mStringData[2];

    LOG_INFO("MPACKET_LOBBY_CREATED received: lobbyId %lu, game '%s', version '%s', title '%s'", mData.lobbyId, game.c_str(), version.c_str(), title.c_str());

    return true;
}

bool MPacketLobbyJoin::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_JOIN received: lobbId %lu", mData.lobbyId);

    Lobby* lobby = gServer->LobbyGet(mData.lobbyId);
    if (lobby && lobby->Join(connection)) {
        // ?
    } else {
        // TODO: send out error packet
    }

    return true;
}

bool MPacketLobbyJoined::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_JOINED received: lobbyId %lu, userId %lu", mData.lobbyId, mData.userId);
    return true;
}

bool MPacketLobbyLeave::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_LEAVE received: lobbyId %lu", mData.lobbyId);

    Lobby* lobby = gServer->LobbyGet(mData.lobbyId);
    if (lobby) {
        lobby->Leave(connection);
    } else {
        // TODO: send out error packet
    }

    return true;
}

bool MPacketLobbyLeft::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_LEFT received: lobbyId %lu, userId %lu", mData.lobbyId, mData.userId);
    return true;
}

bool MPacketLobbyListGet::Receive(Connection* connection) {
    std::string& game = mStringData[0];
    LOG_INFO("MPACKET_LOBBY_LIST_GET received: game '%s'", game.c_str());
    gServer->LobbyListGet(*connection, game);
    return true;
}

bool MPacketLobbyListGot::Receive(Connection* connection) {
    std::string& game = mStringData[0];
    std::string& version = mStringData[1];
    std::string& title = mStringData[2];
    LOG_INFO("MPACKET_LOBBY_LIST_GOT received: lobbyId %lu, ownerId %lu, connections %u/%u, game '%s', version '%s', title '%s'", mData.lobbyId, mData.ownerId, mData.connections, mData.maxConnections, game.c_str(), version.c_str(), title.c_str());
    return true;
}
