#include <map>
#include <stdio.h>
#include <string.h>
#include "peer.hpp"
#include "client.hpp"
#include "mpacket.hpp"
#include "logging.hpp"
#include "types.hpp"
#include <unistd.h> // for sleep

static void sOnStateChanged(juice_agent_t *agent, juice_state_t state, void *user_ptr) { ((Peer*)user_ptr)->OnStateChanged(state); }
static void sOnCandidate(juice_agent_t *agent, const char *sdp, void *user_ptr) { ((Peer*)user_ptr)->OnCandidate(sdp); }
static void sOnGatheringDone(juice_agent_t *agent, void *user_ptr) { ((Peer*)user_ptr)->OnGatheringDone(); }
static void sOnRecv(juice_agent_t *agent, const char *data, size_t size, void *user_ptr) { ((Peer*)user_ptr)->OnRecv(data, size); }

Peer::Peer(Client* client, uint64_t aId) {
    mId = aId;

    juice_set_log_level(JUICE_LOG_LEVEL_WARN);

    // Agent 1: Create agent
    juice_config_t config;
    memset(&config, 0, sizeof(config));

    // STUN server example
    config.stun_server_host = client->mStunServer.host.c_str();
    config.stun_server_port = client->mStunServer.port;

    // TURN server example (use your own server in production)
    mTurnServers = (juice_turn_server_t*)calloc(client->mTurnServers.size(), sizeof(juice_turn_server_t));
    if (!mTurnServers) {
        config.turn_servers = nullptr;
        config.turn_servers_count = 0;
        LOG_ERROR("Failed to allocate turn servers");
    } else {
        for (uint32_t i = 0; i < client->mTurnServers.size(); i++) {
            StunTurnServer* turn = &client->mTurnServers[i];
            mTurnServers[i].host = turn->host.c_str();
            mTurnServers[i].port = turn->port;
            mTurnServers[i].username = turn->username.c_str();
            mTurnServers[i].password = turn->password.c_str();
        }
        config.turn_servers = mTurnServers;
        config.turn_servers_count = client->mTurnServers.size();
    }

	config.local_port_range_begin = 60000;
	config.local_port_range_end = 61000;

    config.cb_state_changed = sOnStateChanged;
    config.cb_candidate = sOnCandidate;
    config.cb_gathering_done = sOnGatheringDone;
    config.cb_recv = sOnRecv;
    config.user_ptr = this;

    mAgent = juice_create(&config);

    // Send if it is an ICE controller
    if (gClient->mCurrentUserId < mId) {
        SendSdp();
    }
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

void Peer::Send(const char* aData, size_t aDataLength) {
    LOG_INFO("Peer sending to (%lu)\n", mId);
    juice_send(mAgent, aData, aDataLength);
}

void Peer::Disconnect() {
    LOG_INFO("Peer disconnect");
    if (mAgent) {
	    juice_destroy(mAgent);
        mAgent = nullptr;
        mTurnServers = nullptr;
    }
}

void Peer::CandidateAdd(const char* aSdp) {
    juice_add_remote_candidate(mAgent, aSdp);
}

void Peer::OnStateChanged(juice_state_t aState) {
    LOG_INFO("State change (%lu): %s", mId, juice_state_to_string(aState));

    if (aState == JUICE_STATE_CONNECTED) {
        // Agent 1: on connected, send a message
        char message[256];
        snprintf(message, 255, "Hello from %lu", gClient->mCurrentUserId);
        juice_send(mAgent, message, strlen(message)+1);
    }

    /////////////////////////

	bool success = (aState == JUICE_STATE_COMPLETED);

    // Retrieve candidates
    char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
    char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
    if (success &=
        (juice_get_selected_candidates(mAgent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote,
                                       JUICE_MAX_CANDIDATE_SDP_STRING_LEN) == 0)) {
        printf("Local candidate  1: %s\n", local);
        printf("Remote candidate 1: %s\n", remote);
        if ((!strstr(local, "typ host") && !strstr(local, "typ prflx")) ||
            (!strstr(remote, "typ host") && !strstr(remote, "typ prflx")))
            success = false; // local connection should be possible
    }

    // Retrieve addresses
    char localAddr[JUICE_MAX_ADDRESS_STRING_LEN];
    char remoteAddr[JUICE_MAX_ADDRESS_STRING_LEN];
    if (success &= (juice_get_selected_addresses(mAgent, localAddr, JUICE_MAX_ADDRESS_STRING_LEN,
                                                 remoteAddr, JUICE_MAX_ADDRESS_STRING_LEN) == 0)) {
        printf("Local address  1: %s\n", localAddr);
        printf("Remote address 1: %s\n", remoteAddr);
    }
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
}
