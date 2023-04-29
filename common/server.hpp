#pragma once

#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <random>
#include <cstdint>
#include <chrono>
#include "socket.hpp"
#include "connection.hpp"
#include "lobby.hpp"

class Server {
    private:
        std::thread mThreadRecv;
        std::thread mThreadUpdate;
        int mSocket;
        std::map<uint64_t, Connection*> mConnections;
        std::mutex mConnectionsMutex;
        std::map<uint64_t, Lobby*> mLobbies;
        std::mt19937_64 mPrng;
        std::uniform_int_distribution<uint64_t> mRng;
        std::vector<StunTurnServer> mTurnServers;
        int mLobbyCount = 0;
        int mPlayerCount = 0;
        bool (*mBanCheckFunction)(uint64_t aDestId, std::string aAddressStr) = nullptr;

        void ReadTurnServers();

    public:
        bool Begin(uint32_t aPort, bool (*aBanCheckFunction)(uint64_t aDestId, std::string aAddressStr));
        void Receive();
        void Update();

        Connection* ConnectionGet(uint64_t aUserId);

        Lobby* LobbyGet(uint64_t aLobbyId);
        void LobbyListGet(Connection& aConnection, std::string aGame, std::string aPassword);

        void OnLobbyJoin(Lobby* aLobby, Connection* aConnection);
        void OnLobbyLeave(Lobby* aLobby, Connection* aConnection);
        void OnLobbyDestroy(Lobby* aLobby);

        void LobbyCreate(Connection* aConnection, std::string& aGame, std::string& aVersion, std::string& aHostName, std::string& aMode, uint16_t aMaxConnections, std::string& aPassword, std::string& aDescription);
        void LobbyUpdate(Connection* aConnection, uint64_t aLobbyId, std::string& aGame, std::string& aVersion, std::string& aHostName, std::string& aMode, std::string& aDescription);

        int PlayerCount();
        int LobbyCount();
};

extern Server* gServer;
