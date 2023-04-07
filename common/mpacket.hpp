#pragma once

#include <cstdint>
#include <vector>
#include "connection.hpp"

#define MPACKET_PROTOCOL_VERSION 1
#define MPACKET_MAX_SIZE ((size_t)1024)

enum MPacketType {
    MPACKET_NONE,
    MPACKET_JOINED,
    MPACKET_LOBBY_CREATE,
    MPACKET_LOBBY_CREATED,
    MPACKET_LOBBY_JOIN,
    MPACKET_LOBBY_JOINED,
    MPACKET_LOBBY_LEAVE,
    MPACKET_LOBBY_LEFT,
    MPACKET_LOBBY_LIST_GET,
    MPACKET_LOBBY_LIST_GOT,
    MPACKET_MAX,
};

#pragma pack(1)

typedef struct {
    uint16_t packetType;
    uint16_t dataSize;
    uint16_t stringSize;
} MPacketHeader;

typedef struct {
    uint64_t userId;
    uint32_t version;
} MPacketJoinedData;

typedef struct {
    uint16_t maxConnections;
} MPacketLobbyCreateData;

typedef struct {
    uint64_t lobbyId;
} MPacketLobbyCreatedData;

typedef struct {
    uint64_t lobbyId;
} MPacketLobbyJoinData;

typedef struct {
    uint64_t lobbyId;
    uint64_t userId;
} MPacketLobbyJoinedData;

typedef struct {
    uint64_t lobbyId;
} MPacketLobbyLeaveData;

typedef struct {
    uint64_t lobbyId;
    uint64_t userId;
} MPacketLobbyLeftData;

typedef struct {
    uint64_t lobbyId;
    uint64_t ownerId;
    uint16_t connections;
    uint16_t maxConnections;
} MPacketLobbyListGotData;

#pragma pack()

class MPacket {
    private:
    protected:
        enum MPacketType mType;

        uint16_t mVoidDataSize = 0;
        void* mVoidData = NULL;

        std::vector<std::string> mStringData;

        MPacket(enum MPacketType aType, void* aData, uint16_t aDataSize) {
            mType = aType;
            mVoidData = aData;
            mVoidDataSize = aDataSize;
        }

    public:
        void Send(Connection& connection);
        void Send(Lobby& lobby);
        static void Read(Connection* connection, uint8_t* aData, uint16_t* aDataSize, uint16_t aMaxDataSize);
        static void Process();
};

class MPacketJoined : public MPacket {
    private:
        MPacketJoinedData mData;
    public:
        MPacketJoined(MPacketJoinedData aData) : MPacket(MPACKET_JOINED, &mData, sizeof(MPacketJoinedData)) { mData = aData; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyCreate : public MPacket {
    private:
        MPacketLobbyCreateData mData;
    public:
        MPacketLobbyCreate(MPacketLobbyCreateData aData, std::vector<std::string> aStringData) : MPacket(MPACKET_LOBBY_CREATE, &mData, sizeof(MPacketLobbyCreateData)) { mData = aData; mStringData = aStringData; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyCreated : public MPacket {
    private:
        MPacketLobbyCreatedData mData;
    public:
        MPacketLobbyCreated(MPacketLobbyCreatedData aData, std::vector<std::string> aStringData) : MPacket(MPACKET_LOBBY_CREATED, &mData, sizeof(MPacketLobbyCreatedData)) { mData = aData; mStringData = aStringData; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyJoin : public MPacket {
    private:
        MPacketLobbyJoinData mData;
    public:
        MPacketLobbyJoin(MPacketLobbyJoinData aData) : MPacket(MPACKET_LOBBY_JOIN, &mData, sizeof(MPacketLobbyJoinData)) { mData = aData; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyJoined : public MPacket {
    private:
        MPacketLobbyJoinedData mData;
    public:
        MPacketLobbyJoined(MPacketLobbyJoinedData aData) : MPacket(MPACKET_LOBBY_JOINED, &mData, sizeof(MPacketLobbyJoinedData)) { mData = aData; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyLeave : public MPacket {
    private:
        MPacketLobbyLeaveData mData;
    public:
        MPacketLobbyLeave(MPacketLobbyLeaveData aData) : MPacket(MPACKET_LOBBY_LEAVE, &mData, sizeof(MPacketLobbyLeaveData)) { mData = aData; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyLeft : public MPacket {
    private:
        MPacketLobbyLeftData mData;
    public:
        MPacketLobbyLeft(MPacketLobbyLeftData aData) : MPacket(MPACKET_LOBBY_LEFT, &mData, sizeof(MPacketLobbyLeftData)) { mData = aData; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyListGet : public MPacket {
    private:
    public:
        MPacketLobbyListGet(std::vector<std::string> aStringData) : MPacket(MPACKET_LOBBY_LIST_GET, NULL, 0) { mStringData = aStringData; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyListGot : public MPacket {
    private:
        MPacketLobbyListGotData mData;
    public:
        MPacketLobbyListGot(MPacketLobbyListGotData data, std::vector<std::string> aStringData) : MPacket(MPACKET_LOBBY_LIST_GOT, &mData, sizeof(MPacketLobbyListGotData)) { mData = data; mStringData = aStringData; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};
