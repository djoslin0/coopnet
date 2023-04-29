#include <vector>
#include <fstream>
#include <mutex>

#include <sys/types.h>
#include <sys/stat.h>
#ifndef WIN32
#include <unistd.h>
#endif

#ifdef WIN32
#define stat _stat
#endif

#include "bansystem.hpp"
#include "logging.hpp"

static const char* sFileName = "banned.cfg";
int64_t sLastModifiedTime = 0;
std::mutex sMutex;

struct BannedPlayer {
    uint64_t destId;
    std::string address;
};
static std::vector<struct BannedPlayer> sBannedPlayers;

static int64_t last_modified_time(const char* filename) {
    struct stat result;
    if (stat(filename, &result) == 0) {
        return result.st_mtime;
    } else {
        return 0;
    }
}

static void ban_system_read() {
    std::lock_guard<std::mutex> guard(sMutex);
    sBannedPlayers.clear();
    std::ifstream input(sFileName);
    std::string line;

    if (input.good()) {
        while (std::getline(input, line)) {
            std::size_t pos1 = line.find(":");
            if (pos1 == std::string::npos) { continue; }

            std::string playerType = line.substr(0, pos1);
            std::string value = line.substr(pos1 + 1);
            if (playerType == "addr") {
                sBannedPlayers.push_back({
                    .destId = 0,
                    .address = value
                });
            } else if (playerType == "dest") {
                sBannedPlayers.push_back({
                    .destId = std::stoull(value),
                    .address = "",
                });
            } else {
                LOG_ERROR("Unrecognized type in %s: %s", sFileName, line.c_str());
            }
        }
        LOG_INFO("Updated %s", sFileName);
    } else {
        LOG_ERROR("%s not found", sFileName);
    }

    input.close();
    sLastModifiedTime = last_modified_time(sFileName);
}

void ban_system_init() {
    ban_system_read();
}

void ban_system_update() {
    if (sLastModifiedTime == 0) { return; }
    int64_t lastModifiedTime = last_modified_time(sFileName);
    if (lastModifiedTime > sLastModifiedTime) {
        ban_system_read();
    }
}

bool ban_system_is_banned(uint64_t aDestId, std::string aAddressStr) {
    std::lock_guard<std::mutex> guard(sMutex);
    for (auto& it : sBannedPlayers) {
        if (it.destId > 0 && it.destId == aDestId) { return true; }
        if (it.address != "" && it.address == aAddressStr) { return true; }
    }
    return false;
}
