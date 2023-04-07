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
} MPacketLobbyListGetData;

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
        void* mVoidData;
        uint16_t mVoidDataSize;
        std::vector<std::string> mStringData;

        virtual enum MPacketType GetPacketType() { return MPACKET_NONE; }

    public:
        void Send(Connection& connection);
        void Send(Lobby& lobby);
        static void Read(Connection* connection, uint8_t* aData, uint16_t* aDataSize, uint16_t aMaxDataSize);
        static void Process();
};

template<typename T>
class MPacketImpl : public MPacket {
    protected:
        T mData;
    public:
        MPacketImpl(T aData) {
            mData = aData;
            mVoidData = &mData;
            mVoidDataSize = sizeof(T);
        }

        MPacketImpl(T aData, std::vector<std::string> aStringData) {
            mData = aData;
            mVoidData = &mData;
            mVoidDataSize = sizeof(T);
            mStringData = aStringData;
        }
};

class MPacketJoined : public MPacketImpl<MPacketJoinedData> {
    public:
        using MPacketImpl::MPacketImpl;
        virtual enum MPacketType GetPacketType() { return MPACKET_JOINED; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyCreate : public MPacketImpl<MPacketLobbyCreateData> {
    public:
        using MPacketImpl::MPacketImpl;
        virtual enum MPacketType GetPacketType() { return MPACKET_LOBBY_CREATE; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyCreated : public MPacketImpl<MPacketLobbyCreatedData> {
    public:
        using MPacketImpl::MPacketImpl;
        virtual enum MPacketType GetPacketType() { return MPACKET_LOBBY_CREATED; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyJoin : public MPacketImpl<MPacketLobbyJoinData> {
    public:
        using MPacketImpl::MPacketImpl;
        virtual enum MPacketType GetPacketType() { return MPACKET_LOBBY_JOIN; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyJoined : public MPacketImpl<MPacketLobbyJoinedData> {
    public:
        using MPacketImpl::MPacketImpl;
        virtual enum MPacketType GetPacketType() { return MPACKET_LOBBY_JOINED; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyLeave : public MPacketImpl<MPacketLobbyLeaveData> {
    public:
        using MPacketImpl::MPacketImpl;
        virtual enum MPacketType GetPacketType() { return MPACKET_LOBBY_LEAVE; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyLeft : public MPacketImpl<MPacketLobbyLeftData> {
    public:
        using MPacketImpl::MPacketImpl;
        virtual enum MPacketType GetPacketType() { return MPACKET_LOBBY_LEFT; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyListGet : public MPacketImpl<MPacketLobbyListGetData> {
    public:
        using MPacketImpl::MPacketImpl;
        virtual enum MPacketType GetPacketType() { return MPACKET_LOBBY_LIST_GET; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};

class MPacketLobbyListGot : public MPacketImpl<MPacketLobbyListGotData> {
    public:
        using MPacketImpl::MPacketImpl;
        virtual enum MPacketType GetPacketType() { return MPACKET_LOBBY_LIST_GOT; }
        static bool Receive(Connection* connection, void* aVoidData, uint16_t aVoidDataSize, std::vector<std::string>& aStringData);
};
