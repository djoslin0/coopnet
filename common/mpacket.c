#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "mpacket.h"

/* structure of mpackets: [32 bit packet length][32 bit param length][param][32 bit param length][param]... */

#define PACKET_LENGTH_SIZE sizeof(uint32_t)

#define PARAM_LENGTH sizeof(uint32_t)

static void mpacket_add_internal(struct MPacket* mp, void* parameter, uint32_t parameterLength) {
    memcpy(&mp->buffer[mp->writeIndex], parameter, parameterLength);
    mp->writeIndex += parameterLength;
}

struct MPacket* mpacket_create() {
    struct MPacket* mp = malloc(sizeof(struct MPacket));
    if (!mp) {
        // TODO: error handling
        return mp;
    }

    mp->bufferLength = 1024;
    mp->buffer = malloc(sizeof(uint8_t) * mp->bufferLength);

    mp->writeIndex = 0;
    mp->readIndex = 0;

    mp->packetLength = 0;

    if (mp->buffer) {
        mpacket_add_internal(mp, &mp->packetLength, PACKET_LENGTH_SIZE);
    }

    return mp;
}

void mpacket_write(struct MPacket* mp, void* parameter, uint32_t parameterLength) {
    // sanity check
    if (!mp || !mp->buffer || !parameter || !parameterLength) {
        // TODO: error handling
        return;
    }

    // increase packet length
    mp->packetLength += parameterLength + PARAM_LENGTH;

    // increase buffer length
    if (mp->packetLength > mp->bufferLength) {
        mp->bufferLength *= 2;
        mp->buffer = realloc(mp->buffer, mp->bufferLength);
        if (!mp->buffer) {
            // TODO: error handling
            return;
        }
    }

    // insert parameter length into buffer
    mpacket_add_internal(mp, &parameterLength, PARAM_LENGTH);

    // insert parameter into buffer
    mpacket_add_internal(mp, parameter, parameterLength);
}

void mpacket_write_u32(struct MPacket* mp, uint32_t parameter) {
    mpacket_write(mp, &parameter, sizeof(uint32_t));
}

void mpacket_write_u64(struct MPacket* mp, uint64_t parameter) {
    mpacket_write(mp, &parameter, sizeof(uint64_t));
}

int mpacket_send(struct MPacket* mp, int socket, struct sockaddr* address) {
    if (!mp || !socket) {
        // TODO: error handling
        return -1;
    }

    // set packet length inside packet
    memcpy(mp->buffer, &mp->packetLength, PACKET_LENGTH_SIZE);

    // send packet
    int ret = sendto(socket, mp->buffer, mp->packetLength, 0, address, sizeof(struct sockaddr));

    if (ret < 0) {
        // TODO: error handling
    }  

    return ret;
}

void mpacket_free(struct MPacket* mp) {
    if (!mp) {
        // TODO: error handling
        return;
    }

    if (mp->buffer) { free (mp->buffer); }

    mp->buffer = NULL;
    mp->bufferLength = 0;
    mp->writeIndex = 0;
    mp->readIndex = 0;
    mp->packetLength = 0;

    free(mp);
}

void mpacket_print(struct MPacket* mp) {
    int i = 0;
    int packetLength = *((int32_t*)&mp->buffer[i]);
    i += sizeof(int32_t);
    printf("\nPacket Length: %u\n", packetLength);

    while (i < mp->packetLength) {
        int paramLength = *((int32_t*)&mp->buffer[i]);
        i += sizeof(int32_t);
        printf("  Param Length: %u\n    ", paramLength);
        int j = i;
        i += paramLength;
        while (j < i) {
            printf("%02X ", mp->buffer[j]);
            j++;
        }
        printf("\n");
    }

    printf("\n");
}