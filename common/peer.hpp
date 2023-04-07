#pragma once

#include <stdint.h>
#include "juice/juice.h"

class Peer {
    private:
        uint64_t mId;
        juice_agent_t* mAgent = nullptr;

        void SendSdp();

    public:
        char mSdp[JUICE_MAX_SDP_STRING_LEN];

        Peer(uint64_t aId);

        void Connect(const char* aSdp);
        void Disconnect();
        void CandidateAdd(const char* aSdp);
        void Send(const char* aData, size_t aDataLength);

        void OnStateChanged(juice_state_t aState);
        void OnCandidate(const char* aSdp);
        void OnGatheringDone();
        void OnRecv(const char* aData, size_t aSize);
};
