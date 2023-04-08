#pragma once

#include <map>
#include <stdint.h>
#include "connection.hpp"
#include "peer.hpp"
#include "types.hpp"

class Client {
    private:
        std::map<uint64_t, Peer*> mPeers;
    public:
        uint64_t mCurrentUserId = 0;
        uint64_t mCurrentLobbyId = 0;
        Connection* mConnection = nullptr;

        StunTurnServer mStunServer;
        std::vector<StunTurnServer> mTurnServers;

        ~Client();

        bool Begin(std::string aHost, uint32_t aPort);
        void Update();
        void Disconnect();

        void PeerBegin(uint64_t userId);
        void PeerEnd(uint64_t userId);
        void PeerEndAll();
        Peer* PeerGet(uint64_t userId);
        bool PeerSend(const uint8_t* aData, size_t aDataLength);
        bool PeerSendTo(uint64_t aPeerId, const uint8_t* aData, size_t aDataLength);

        void LobbyCreate(std::string aGame, std::string aVersion, std::string aTitle, uint16_t aMaxConnections);
        void LobbyJoin(uint64_t aLobbyId);
        void LobbyLeave(uint64_t aLobbyId);
        void LobbyListGet(std::string aGame);
};

extern Client* gClient;
