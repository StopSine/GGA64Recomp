#ifndef __GOEMON_CONFIG_H__
#define __GOEMON_CONFIG_H__

#include <string_view>

namespace goemon64 {
    // Names the save, config and mod directory under the platform's appdata
    // location, so changing it orphans anything already stored there.
    constexpr std::u8string_view program_id = u8"GGA64Recomp";
    constexpr std::string_view program_name = "Goemon's Great Adventure: Recompiled";

    void init_config();
    bool get_noclip_enabled();
    bool get_speed_boost_enabled();
    bool get_invincible_enabled();

    void toggle_noclip();
    void toggle_invincible();
};

#endif
