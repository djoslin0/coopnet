#pragma once

#include <cinttypes>
#include <string>

void ban_system_init();
void ban_system_update();
bool ban_system_is_banned(uint64_t aDestId, std::string aAddressStr);
