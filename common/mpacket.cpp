#include <cstdint>
#include <map>
#include <string>
#include <errno.h>
#include "libcoopnet.h"
#include "socket.hpp"
#include "mpacket.hpp"
#include "logging.hpp"
#include "server.hpp"
#include "client.hpp"
#include "utils.hpp"

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
    new MPacketLobbyListFinish(),
    new MPacketPeerSdp(),
    new MPacketPeerCandidate(),
    new MPacketPeerFailed(),
    new MPacketStunTurn(),
    new MPacketError(),
    new MPacketLobbyUpdate(),
};

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
        LOG_ERROR("Packet size exceeded max size (%" PRIu64 " > %" PRIu64 ")", (uint64_t)dataSize, (uint64_t)MPACKET_MAX_SIZE);
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
    size_t sent = sendto(connection.mSocket, (char*)&data[0], dataSize, MSG_NOSIGNAL, (const sockaddr*)&connection.mAddress, sizeof(struct sockaddr_in));
    int rc = SOCKET_LAST_ERROR;

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
        LOG_ERROR("Error sending data, did not send all bytes (%" PRIu64 " != %" PRIu64 ")!", (uint64_t)sent, (uint64_t)dataSize);
    }
}

void MPacket::Send(Lobby& lobby) {
    for (auto& it : lobby.mConnections) {
        Send(*it);
    }
}

void MPacket::Process(Connection* connection, uint8_t* aData) {
    // make sure connection is still valid
    if (!Connection::IsValid(connection)) { return; }

    // extract variables from data
    MPacketHeader header = *(MPacketHeader*)aData;
    void* voidData = &aData[sizeof(MPacketHeader)];
    void* stringData = &aData[sizeof(MPacketHeader) + header.dataSize];
    bool parseError = false;

    /*LOG_INFO("Processing data:");
    for (size_t i = 0; i < (size_t)(sizeof(MPacketHeader) + header.dataSize + header.stringSize); i++) {
        printf("  %02X", ((uint8_t*)aData)[i]);
    }
    printf("\n");*/

    // sanity check packet type
    if (header.packetType >= MPACKET_MAX || header.packetType == MPACKET_NONE) {
        LOG_ERROR("Received an unknown packet type: %u, data size: %u, string size: %u", header.packetType, header.dataSize, header.stringSize);
        return;
    }

    // receive packet
    MPacket* packet = sPacketByType[header.packetType];

    // sanity check data size
    if (header.dataSize != packet->mVoidDataSize) {
        LOG_ERROR("Received the wrong data size: %u != %u (packetType %u)", header.dataSize, packet->mVoidDataSize, header.packetType);
        return;
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
        if (c >= climit) {
            if (length == 0) {
                packet->mStringData.push_back("");
                break;
            }
            parseError = true;
            break;
        }

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
        return;
    }
    if (packet->mStringData.size() != impl.stringCount) {
        LOG_ERROR("Received packet string count mismatch: %" PRIu64 " != %u", (uint64_t)packet->mStringData.size(), impl.stringCount);
        return;
    }
    if (gServer && impl.sendType == MSEND_TYPE_SERVER) {
        LOG_ERROR("Received server packet while being a server!");
        return;
    }
    if (gClient && impl.sendType == MSEND_TYPE_CLIENT) {
        LOG_ERROR("Received client packet while being a client!");
        return;
    }

    // receive the packet
    if (parseError) {
        LOG_ERROR("Packet parse error!");
    } else {
        bool ret = packet->Receive(connection);
        if (!ret) { LOG_ERROR("Packet receive error %u!", header.packetType); }
    }
}

void MPacket::Read(Connection* connection, uint8_t* aData, uint16_t* aDataSize, uint16_t aMaxDataSize) {
    while (true) {
        MPacketHeader header = *(MPacketHeader*)aData;
        uint16_t totalSize = sizeof(MPacketHeader) + header.dataSize + header.stringSize;

        // check the received size
        if (*aDataSize < totalSize) {
            return;
        }

        // process
        MPacket::Process(connection, aData);

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
    LOG_INFO("MPACKET_JOINED received: userID %" PRIu64 ", version %u", mData.userId, mData.version);
    if (mData.version != MPACKET_PROTOCOL_VERSION) {
        if (gCoopNetCallbacks.OnError) {
            gCoopNetCallbacks.OnError(MERR_COOPNET_VERSION);
        }
        gClient->Disconnect();
        return false;
    }

    gClient->mCurrentUserId = mData.userId;

    if (gCoopNetCallbacks.OnConnected) {
        gCoopNetCallbacks.OnConnected(mData.userId);
    }

    return true;
}

bool MPacketLobbyCreate::Receive(Connection* connection) {
    std::string game        = mStringData[0].substr(0, 32);
    std::string version     = mStringData[1].substr(0, 32);
    std::string hostName    = mStringData[2].substr(0, 32);
    std::string mode        = mStringData[3].substr(0, 32);
    std::string password    = mStringData[4].substr(0, 64);
    std::string description = mStringData[5].substr(0, 256);

    LOG_INFO("MPACKET_LOBBY_CREATE received: game '%s', version '%s', hostName '%s', mode '%s', maxconnections %u, password '%s'",
        game.c_str(), version.c_str(), hostName.c_str(), mode.c_str(), mData.maxConnections, password.c_str());
    gServer->LobbyCreate(connection, game, version, hostName, mode, mData.maxConnections, password, description);

    return true;
}

bool MPacketLobbyCreated::Receive(Connection* connection) {
    std::string game     = mStringData[0].substr(0, 32);
    std::string version  = mStringData[1].substr(0, 32);
    std::string hostName = mStringData[2].substr(0, 32);
    std::string mode     = mStringData[3].substr(0, 32);

    LOG_INFO("MPACKET_LOBBY_CREATED received: lobbyId %" PRIu64 ", game '%s', version '%s', hostName '%s', mode '%s', maxConnections %" PRIu64 "",
        mData.lobbyId, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str(), mData.maxConnections);

    if (gCoopNetCallbacks.OnLobbyCreated) {
        gCoopNetCallbacks.OnLobbyCreated(mData.lobbyId, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str(), mData.maxConnections);
    }

    return true;
}

bool MPacketLobbyJoin::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_JOIN received: lobbyId %" PRIu64 "", mData.lobbyId);

    std::string password = mStringData[0].substr(0, 64);

    Lobby* lobby = gServer->LobbyGet(mData.lobbyId);
    if (!lobby) {
        MPacketError({ .errorNumber = MERR_LOBBY_NOT_FOUND }).Send(*connection);
        return false;
    }

    enum MPacketErrorNumber rc = lobby->Join(connection, password);
    if (rc != MERR_NONE) {
        MPacketError({ .errorNumber = (uint16_t)rc }).Send(*connection);
        return false;
    }

    return true;
}

bool MPacketLobbyJoined::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_JOINED received: lobbyId %" PRIu64 ", userId %" PRIu64 ", priority %u, ownerId %" PRIu64 ", destId %" PRIu64 "",
        mData.lobbyId, mData.userId, mData.priority, mData.ownerId, mData.destId);

    if (mData.userId == gClient->mCurrentUserId) {
        gClient->mCurrentLobbyId = mData.lobbyId;
        gClient->mCurrentPriority = mData.priority;
    } else if (mData.lobbyId == gClient->mCurrentLobbyId) {
        gClient->PeerBegin(mData.userId, mData.priority);
    } else {
        LOG_ERROR("Received 'joined' for the wrong lobby");
        return false;
    }

    if (gCoopNetCallbacks.OnLobbyJoined) {
        gCoopNetCallbacks.OnLobbyJoined(mData.lobbyId, mData.userId, mData.ownerId, mData.destId);
    }

    return true;
}

bool MPacketLobbyLeave::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_LEAVE received: lobbyId %" PRIu64 "", mData.lobbyId);

    Lobby* lobby = gServer->LobbyGet(mData.lobbyId);
    if (!lobby) {
        MPacketError({ .errorNumber = MERR_LOBBY_NOT_FOUND }).Send(*connection);
        return false;
    }

    lobby->Leave(connection);

    return true;
}

bool MPacketLobbyLeft::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_LEFT received: lobbyId %" PRIu64 ", userId %" PRIu64 "", mData.lobbyId, mData.userId);

    if (mData.userId == gClient->mCurrentUserId) {
        gClient->mCurrentLobbyId = 0;
        gClient->mCurrentPriority = 0;
        gClient->PeerEndAll();
    } else if (mData.lobbyId == gClient->mCurrentLobbyId) {
        gClient->PeerEnd(mData.userId);
    } else {
        LOG_ERROR("Received 'left' for the wrong lobby");
    }

    if (gCoopNetCallbacks.OnLobbyLeft) {
        gCoopNetCallbacks.OnLobbyLeft(mData.lobbyId, mData.userId);
    }

    return true;
}

bool MPacketLobbyListGet::Receive(Connection* connection) {
    std::string game     = mStringData[0].substr(0, 32);
    std::string password = mStringData[1].substr(0, 64);
    LOG_INFO("MPACKET_LOBBY_LIST_GET received: game '%s'", game.c_str());
    gServer->LobbyListGet(*connection, game, password);
    return true;
}

bool MPacketLobbyListGot::Receive(Connection* connection) {
    std::string game        = mStringData[0].substr(0, 32);
    std::string version     = mStringData[1].substr(0, 32);
    std::string hostName    = mStringData[2].substr(0, 32);
    std::string mode        = mStringData[3].substr(0, 32);
    std::string description = mStringData[4].substr(0, 256);
    LOG_INFO("MPACKET_LOBBY_LIST_GOT received: lobbyId %" PRIu64 ", ownerId %" PRIu64 ", connections %u/%u, game '%s', version '%s', hostname '%s', mode '%s'",
        mData.lobbyId, mData.ownerId, mData.connections, mData.maxConnections, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str());

    if (gCoopNetCallbacks.OnLobbyListGot) {
        gCoopNetCallbacks.OnLobbyListGot(mData.lobbyId, mData.ownerId, mData.connections, mData.maxConnections, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str(), description.c_str());
    }

    return true;
}

bool MPacketLobbyListFinish::Receive(Connection* connection) {
    LOG_INFO("MPACKET_LOBBY_LIST_FINISH received");

    if (gCoopNetCallbacks.OnLobbyListFinish) {
        gCoopNetCallbacks.OnLobbyListFinish();
    }

    return true;
}

bool MPacketPeerSdp::Receive(Connection *connection) {
    std::string& sdp = mStringData[0];
    LOG_INFO("MPACKET_PEER_SDP received: lobbyId %" PRIu64 ", userId %" PRIu64 ", sdp '%s'", mData.lobbyId, mData.userId, sdp.c_str());
    if (gServer) {
        Connection* other = gServer->ConnectionGet(mData.userId);

        if (!other) {
            LOG_ERROR("Could not find user: %" PRIu64 "", mData.userId);
            return false;
        }

        MPacketPeerSdp({
           .lobbyId = mData.lobbyId,
           .userId = connection->mId
        }, mStringData).Send(*other);

        return true;
    }

    if (gClient) {
        Peer* peer = gClient->PeerGet(mData.userId);

        if (!peer) {
            LOG_ERROR("Could not find peer: %" PRIu64 "", mData.userId);
            return false;
        }

        peer->Connect(sdp.c_str());
        return true;
    }

    LOG_ERROR("Received peer sdp without being server or client");
    return false;
}

bool MPacketPeerCandidate::Receive(Connection *connection) {
    std::string& sdp = mStringData[0];
    LOG_INFO("MPACKET_PEER_CANDIDATE received: lobbyId %" PRIu64 ", userId %" PRIu64 ", sdp '%s'", mData.lobbyId, mData.userId, sdp.c_str());
    if (gServer) {
        Connection* other = gServer->ConnectionGet(mData.userId);

        if (!other) {
            LOG_ERROR("Could not find user: %" PRIu64 "", mData.userId);
            return false;
        }

        MPacketPeerCandidate({
           .lobbyId = mData.lobbyId,
           .userId = connection->mId
        }, mStringData).Send(*other);

        return true;
    }

    if (gClient) {
        Peer* peer = gClient->PeerGet(mData.userId);

        if (!peer) {
            LOG_ERROR("Could not find peer: %" PRIu64 "", mData.userId);
            return false;
        }

        peer->CandidateAdd(sdp.c_str());
        return true;
    }

    LOG_ERROR("Received peer sdp without being server or client");
    return false;
}

bool MPacketPeerFailed::Receive(Connection *connection) {
    LOG_INFO("MPACKET_PEER_FAILED received: lobbyId %" PRIu64 ", peerId %" PRIu64 "", mData.lobbyId, mData.peerId);
    // make sure client is still in this lobby
    if (!connection->mLobby || connection->mLobby->mId != mData.lobbyId) {
        LOG_ERROR("Peer failed, but the one that saw the failure is no longer in the lobby");
        return false;
    }

    // make sure peer is still in this lobby
    Connection* peer = gServer->ConnectionGet(mData.peerId);
    if (!peer || peer->mLobby->mId != mData.lobbyId) {
        LOG_ERROR("Peer failed, but the peer of the one that saw the failure is no longer in the lobby");
        return false;
    }

    // check the priority of the two connections
    if (peer->mPriority <= connection->mPriority) {
        LOG_ERROR("Peer failed, but the priority was incorrect");
        return false;
    }

    // kick them
    connection->mLobby->Leave(peer);

    return true;
}

bool MPacketStunTurn::Receive(Connection* connection) {
    std::string host     = mStringData[0];
    std::string username = mStringData[1];
    std::string password = mStringData[2];
    LOG_INFO("MPACKET_STUN_TURN received: isStun %u, host '%s', port %u, username '%s', password '%s'", mData.isStun, host.c_str(), mData.port, username.c_str(), password.c_str());

    if (mData.isStun) {
        gClient->mStunServer.host = host;
        gClient->mStunServer.port = mData.port;
    } else {
        gClient->mTurnServers.push_back({
            .host = host,
            .username = username,
            .password = password,
            .port = mData.port,
        });
    }

    return true;
}

bool MPacketError::Receive(Connection* connection) {
    LOG_INFO("MPACKET_ERROR received: errno %u", mData.errorNumber);
    if (gCoopNetCallbacks.OnError) {
        gCoopNetCallbacks.OnError((enum MPacketErrorNumber)mData.errorNumber);
    }
    return true;
}

bool MPacketLobbyUpdate::Receive(Connection* connection) {
    std::string game        = mStringData[0].substr(0, 32);
    std::string version     = mStringData[1].substr(0, 32);
    std::string hostName    = mStringData[2].substr(0, 32);
    std::string mode        = mStringData[3].substr(0, 32);
    std::string description = mStringData[4].substr(0, 256);

    LOG_INFO("MPACKET_LOBBY_UPDATE received: lobbyId %" PRIu64 ", game '%s', version '%s', hostName '%s', mode '%s'",
        mData.lobbyId, game.c_str(), version.c_str(), hostName.c_str(), mode.c_str());
    gServer->LobbyUpdate(connection, mData.lobbyId, game, version, hostName, mode, description);

    return true;
}
