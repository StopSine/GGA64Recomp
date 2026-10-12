#ifndef __GOEMON_GAME_H__
#define __GOEMON_GAME_H__

#include <cstdint>
#include <span>
#include <vector>

#include "ultramodern/input.hpp"

namespace goemon64 {
    ultramodern::input::connected_device_info_t get_connected_device_info(int controller_num);

    void value_search_poll(uint8_t* rdram);
    void debug_move_poll(uint8_t* rdram);
    bool noclip_is_enabled();

    void quicksave_save();
    void quicksave_load();
    std::vector<uint8_t> decompress_gga(std::span<const uint8_t> compressed_rom);
};

#endif
