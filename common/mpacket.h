#ifndef MPACKET_H
#define MPACKET_H

#include <stdint.h>

struct MPacket {
    uint8_t* buffer;
    uint32_t bufferLength;
    uint32_t writeIndex;
    uint32_t readIndex;
    uint32_t packetLength;
};

enum MPacketType {
    MPACKET_USER_ID,
};

struct MPacket* mpacket_create();

void mpacket_write(struct MPacket* mp, void* parameter, uint32_t parameterLength);
void mpacket_write_u32(struct MPacket* mp, uint32_t parameter);
void mpacket_write_u64(struct MPacket* mp, uint64_t parameter);

int mpacket_send(struct MPacket* mp, int socket, struct sockaddr* address);
void mpacket_free(struct MPacket* mp);
void mpacket_print(struct MPacket* mp);

#endif