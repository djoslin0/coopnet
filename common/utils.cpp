#include <stdexcept>
#include <string>
#include <time.h>
#include "socket.hpp"

// Convert a domain name to an in_addr using gethostbyname
in_addr_t GetAddrFromDomain(const std::string& domain) {
    struct hostent* he = gethostbyname(domain.c_str());
    if (he == nullptr) {
        he = gethostbyname("127.0.0.1");
    }
    struct in_addr** addr_list = reinterpret_cast<in_addr**>(he->h_addr_list);
    return addr_list[0]->s_addr;
}

static void _clock_gettime(struct timespec* clock_time) {
#if !defined _POSIX_MONOTONIC_CLOCK || _POSIX_MONOTONIC_CLOCK < 0
    clock_gettime(CLOCK_REALTIME, clock_time);
#elif _POSIX_MONOTONIC_CLOCK > 0
    clock_gettime(CLOCK_MONOTONIC, clock_time);
#else
    if (clock_gettime(CLOCK_MONOTONIC, clock_time)) {
        clock_gettime(CLOCK_REALTIME, clock_time);
    }
#endif
}

static uint64_t clock_elapsed_ns(void) {
    static bool sClockInitialized = false;
    static uint64_t clock_start_ns;
    if (!sClockInitialized) {
        struct timespec clock_start;
        _clock_gettime(&clock_start);
        clock_start_ns = ((uint64_t)clock_start.tv_sec) * 1000000000 + ((uint64_t)clock_start.tv_nsec);
        sClockInitialized = true;
    }

    struct timespec clock_current;
    _clock_gettime(&clock_current);

    uint64_t clock_current_ns = ((uint64_t)clock_current.tv_sec) * 1000000000 + ((uint64_t)clock_current.tv_nsec);
    return (clock_current_ns - clock_start_ns);
}

float clock_elapsed(void) {
    return (clock_elapsed_ns() / 1000000000.0f);
}