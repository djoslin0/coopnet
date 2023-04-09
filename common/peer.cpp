#include <map>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "coopnet.h"
#include "peer.hpp"
#include "client.hpp"
#include "mpacket.hpp"
#include "logging.hpp"
#include "utils.hpp"

#define PEER_TIMEOUT 15.0f /* 15 seconds */

static void sOnStateChanged(juice_agent_t *agent, juice_state_t state, void *user_ptr) { ((Peer*)user_ptr)->OnStateChanged(state); }
static void sOnCandidate(juice_agent_t *agent, const char *sdp, void *user_ptr) { ((Peer*)user_ptr)->OnCandidate(sdp); }
static void sOnGatheringDone(juice_agent_t *agent, void *user_ptr) { ((Peer*)user_ptr)->OnGatheringDone(); }
static void sOnRecv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) { ((Peer*)user_ptr)->OnRecv(data, size); }

Peer::Peer(Client* aClient, uint64_t aId, uint32_t aPriority) {
    mId = aId;
    mPriority = aPriority;
    mLastState = JUICE_STATE_DISCONNECTED;
    mCurrentState = JUICE_STATE_DISCONNECTED;
    mTimeout = clock_elapsed() + PEER_TIMEOUT;

    juice_set_log_level(JUICE_LOG_LEVEL_WARN);

    // Agent 1: Create agent
    juice_config_t config;
    memset(&config, 0, sizeof(config));

    // STUN server example
    config.stun_server_host = aClient->mStunServer.host.c_str();
    config.stun_server_port = aClient->mStunServer.port;

    // TURN server example (use your own server in production)
    mTurnServers = (juice_turn_server_t*)calloc(aClient->mTurnServers.size(), sizeof(juice_turn_server_t));
    if (!mTurnServers) {
        config.turn_servers = nullptr;
        config.turn_servers_count = 0;
        LOG_ERROR("Failed to allocate turn servers");
    } else {
        for (uint32_t i = 0; i < aClient->mTurnServers.size(); i++) {
            StunTurnServer* turn = &aClient->mTurnServers[i];
            mTurnServers[i].host = turn->host.c_str();
            mTurnServers[i].port = turn->port;
            mTurnServers[i].username = turn->username.c_str();
            mTurnServers[i].password = turn->password.c_str();
        }
        config.turn_servers = mTurnServers;
        config.turn_servers_count = aClient->mTurnServers.size();
    }

	config.local_port_range_begin = 60000;
	config.local_port_range_end = 61000;

    config.cb_state_changed = sOnStateChanged;
    config.cb_candidate = sOnCandidate;
    config.cb_gathering_done = sOnGatheringDone;
    config.cb_recv = sOnRecv;
    config.user_ptr = this;

    mConnected = false;
    mAgent = juice_create(&config);

    // Send if it is an ICE controller
    if (gClient->mCurrentUserId < mId) {
        SendSdp();
    }
}

void Peer::Update() {
    if (mConnected) { return; }
    if (mPriority < gClient->mCurrentPriority) { return; }

    float now = clock_elapsed();
    if (now < mTimeout) { return; }
    mTimeout = now + PEER_TIMEOUT;

    LOG_INFO("Peer '%lu' failed", mId);

    MPacketPeerFailed(
        { .lobbyId = gClient->mCurrentLobbyId, .peerId = mId }
    ).Send(*gClient->mConnection);
}

void Peer::Connect(const char* aSdp) {
    juice_set_remote_description(mAgent, aSdp);

    // Send if it isn't an ICE controller
    if (gClient->mCurrentUserId >= mId) {
        SendSdp();
    }
}

void Peer::SendSdp() {
    juice_get_local_description(mAgent, mSdp, JUICE_MAX_SDP_STRING_LEN);
    LOG_INFO("Local description (%lu):\n%s\n", mId, mSdp);

    MPacketPeerSdp(
        { .lobbyId = gClient->mCurrentLobbyId, .userId = mId },
        { mSdp }
    ).Send(*gClient->mConnection);

    juice_gather_candidates(mAgent);
}

bool Peer::Send(const uint8_t* aData, size_t aDataLength) {
    LOG_INFO("Peer sending to (%lu)\n", mId);
    juice_send(mAgent, (const char*)aData, aDataLength);
    return true;
}

void Peer::Disconnect() {
    if (mAgent) {
        LOG_INFO("Peer disconnect");
	    juice_destroy(mAgent);
        mAgent = nullptr;
        mTurnServers = nullptr;
        mConnected = false;
    }
}

void Peer::CandidateAdd(const char* aSdp) {
    juice_add_remote_candidate(mAgent, aSdp);
}

void Peer::OnStateChanged(juice_state_t aState) {
    LOG_INFO("State change (%lu): %s", mId, juice_state_to_string(aState));

    bool wasConnected = (mLastState == JUICE_STATE_CONNECTED) || (mLastState == JUICE_STATE_COMPLETED);
    bool wasDisconnected = (mLastState == JUICE_STATE_DISCONNECTED) || (mLastState == JUICE_STATE_FAILED);

    mConnected = (aState == JUICE_STATE_CONNECTED) || (aState == JUICE_STATE_COMPLETED);
    bool isDisconnected = (aState == JUICE_STATE_DISCONNECTED) || (aState == JUICE_STATE_FAILED);

    if (!wasConnected && mConnected) {
        if (gCoopNetCallbacks.OnPeerConnected) {
            gCoopNetCallbacks.OnPeerConnected(mId);
        }
    }

    if (!wasDisconnected && isDisconnected) {
        if (gCoopNetCallbacks.OnPeerDisconnected) {
            gCoopNetCallbacks.OnPeerDisconnected(mId);
        }
    }

    mLastState = mCurrentState;
    mCurrentState = aState;
}

void Peer::OnCandidate(const char* aSdp) {
    LOG_INFO("Candidate (%lu): %s", mId, aSdp);

    MPacketPeerCandidate(
        { .lobbyId = gClient->mCurrentLobbyId, .userId = mId },
        { aSdp }
    ).Send(*gClient->mConnection);
}

void Peer::OnGatheringDone() {
    LOG_INFO("Gathering done (%lu)", mId);
}

void Peer::OnRecv(const char* aData, size_t aSize) {
    LOG_INFO("Recv (%lu), size %lu: %s", mId, aSize, aData);

    if (gCoopNetCallbacks.OnReceive) {
        gCoopNetCallbacks.OnReceive(mId, (const uint8_t*)aData, aSize);
    }
}
