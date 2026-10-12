#include <cstdint>
#include <cstring>
#include <cmath>

#include <SDL.h>

#include "recomp.h"
#include "goemon_config.h"
#include "goemon_game.h"

namespace {

// players[4], from player_slot_occupied at 0x8018B178.
constexpr uint32_t player_table_guest = 0x800883A0;

// func_80194A8C copies this into the render node every frame, so it is the
// position every other copy derives from. Offsets are from the player struct,
// which the movement code addresses as player+0x20.
constexpr uint32_t player_position_offset = 0x70;
// func_80185BC0 feeds these to the 2.5D integrator; left alone, collision
// contributes a delta that cancels the write.
constexpr uint32_t player_move_input_x_offset = 0x118;
constexpr uint32_t player_move_input_z_offset = 0x124;
constexpr uint32_t player_gravity_offset = 0x20C;
constexpr uint32_t player_walk_speed_offset = 0x208;

// func_801893CC dispatches the player state through this table, indexed by the
// state at obj+0x8. Swapping the hurt handler for a bare `jr $ra` stops the
// damage reaction without patching anything.
constexpr uint32_t player_state_table_guest = 0x801A7670;
constexpr uint32_t player_state_table_entries = 64;
constexpr uint32_t hurt_handler_guest = 0x80176558;

constexpr float noclip_step = 8.0f;
constexpr float noclip_fast_multiplier = 5.0f;
constexpr float boosted_walk_speed = 12.0f;

bool f5_was_down = false;
bool f6_was_down = false;
bool no_damage_was_enabled = false;
bool noclip_was_enabled = false;
bool speed_was_enabled = false;
float stock_gravity = 0.0f;
float stock_walk_speed = 0.0f;

inline int64_t as_reg(uint32_t guest) {
    return (int64_t)(int32_t)guest;
}

uint32_t read_u32(uint8_t* rdram, uint32_t guest) {
    return (uint32_t)MEM_W(0, as_reg(guest));
}

// A guest pointer that is not KSEG0 would fault the host once dereferenced.
bool valid_guest(uint32_t guest) {
    return guest >= 0x80000000u && guest < 0x80800000u;
}

float read_float(uint8_t* rdram, uint32_t guest) {
    uint32_t bits = read_u32(rdram, guest);
    float out;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}

void write_float(uint8_t* rdram, uint32_t guest, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    MEM_W(0, as_reg(guest)) = (int32_t)bits;
}

// A dead handler leaves the player stuck in the hurt state, so the substitute
// is the table's first entry, the normal state, which transitions out itself.
void set_hurt_handler(uint8_t* rdram, uint32_t from, uint32_t to) {
    for (uint32_t i = 1; i < player_state_table_entries; i++) {
        uint32_t slot = player_state_table_guest + i * 4;
        if (read_u32(rdram, slot) == from) {
            MEM_W(0, as_reg(slot)) = (int32_t)to;
        }
    }
}

uint32_t normal_handler(uint8_t* rdram) {
    return read_u32(rdram, player_state_table_guest);
}

}

bool goemon64::noclip_is_enabled() {
    return goemon64::get_noclip_enabled();
}

void goemon64::debug_move_poll(uint8_t* rdram) {
    {
        int hotkeys = 0;
        const uint8_t* state = SDL_GetKeyboardState(&hotkeys);
        if (state != nullptr && hotkeys > SDL_SCANCODE_F6) {
            bool f5 = state[SDL_SCANCODE_F5] != 0;
            if (f5 && !f5_was_down) {
                goemon64::toggle_noclip();
            }
            f5_was_down = f5;

            bool f6 = state[SDL_SCANCODE_F6] != 0;
            if (f6 && !f6_was_down) {
                goemon64::toggle_invincible();
            }
            f6_was_down = f6;
        }
    }

    bool noclip = goemon64::get_noclip_enabled();
    bool speed_boost = goemon64::get_speed_boost_enabled();

    uint32_t player = read_u32(rdram, player_table_guest);
    if (!valid_guest(player)) {
        return;
    }

    uint32_t speed_at = player + player_walk_speed_offset;
    if (speed_boost != speed_was_enabled) {
        if (speed_boost) {
            stock_walk_speed = read_float(rdram, speed_at);
        }
        else if (stock_walk_speed > 0.0f) {
            write_float(rdram, speed_at, stock_walk_speed);
        }
        speed_was_enabled = speed_boost;
    }
    if (speed_boost) {
        write_float(rdram, speed_at, boosted_walk_speed);
    }

    // Loading a level restores the table from ROM, so this reapplies rather
    // than acting only on the toggle changing.
    bool no_damage = goemon64::get_invincible_enabled();
    if (no_damage) {
        set_hurt_handler(rdram, hurt_handler_guest, normal_handler(rdram));
    }
    else if (no_damage_was_enabled) {
        set_hurt_handler(rdram, normal_handler(rdram), hurt_handler_guest);
    }
    no_damage_was_enabled = no_damage;

    uint32_t gravity_at = player + player_gravity_offset;
    if (noclip != noclip_was_enabled) {
        if (noclip) {
            stock_gravity = read_float(rdram, gravity_at);
        }
        else if (stock_gravity != 0.0f) {
            write_float(rdram, gravity_at, stock_gravity);
        }
        noclip_was_enabled = noclip;
    }
    if (!noclip) {
        return;
    }
    write_float(rdram, gravity_at, 0.0f);

    int numkeys = 0;
    const uint8_t* keys = SDL_GetKeyboardState(&numkeys);
    if (keys == nullptr || numkeys <= SDL_SCANCODE_RSHIFT) {
        return;
    }

    // Sharing the game's stick keys is safe: noclip stops the engine applying
    // its own move, so the stick cannot move the player.
    float dx = 0.0f, dy = 0.0f, dz = 0.0f;
    if (keys[SDL_SCANCODE_W]) dz -= 1.0f;
    if (keys[SDL_SCANCODE_S]) dz += 1.0f;
    if (keys[SDL_SCANCODE_A]) dx -= 1.0f;
    if (keys[SDL_SCANCODE_D]) dx += 1.0f;
    if (keys[SDL_SCANCODE_E]) dy += 1.0f;
    if (keys[SDL_SCANCODE_Q]) dy -= 1.0f;

    if (dx == 0.0f && dy == 0.0f && dz == 0.0f) {
        return;
    }

    float step = noclip_step;
    if (keys[SDL_SCANCODE_RSHIFT]) {
        step *= noclip_fast_multiplier;
    }

    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    dx = dx / len * step;
    dy = dy / len * step;
    dz = dz / len * step;

    write_float(rdram, player + player_move_input_x_offset, 0.0f);
    write_float(rdram, player + player_move_input_z_offset, 0.0f);

    uint32_t pos = player + player_position_offset;
    write_float(rdram, pos + 0, read_float(rdram, pos + 0) + dx);
    write_float(rdram, pos + 4, read_float(rdram, pos + 4) + dy);
    write_float(rdram, pos + 8, read_float(rdram, pos + 8) + dz);
}
