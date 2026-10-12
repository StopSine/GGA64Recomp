// Temporary. Differential search for a live gameplay value, to find the
// function that writes it. F9 steps the search, F10 resets it.

#include <cstdio>
#include <cstdint>
#include <vector>

#include <SDL.h>

#include "goemon_game.h"

namespace {

constexpr uint32_t search_size = 0x400000;

// Item counts, indexed by item id; entry passes are id 0. Written by
// func_800ED10C_5A832C, which clamps against the caps at D_8010AEB8_5C60D8.
constexpr uint32_t entry_pass_guest = 0x80088275;

std::vector<uint8_t> snapshot;
std::vector<uint32_t> candidates;
bool have_snapshot = false;
bool f7_was_down = false;
bool f8_was_down = false;
bool f9_was_down = false;
bool f10_was_down = false;

// The save payload in RAM. Verified against the pak image at two points: the
// entry pass count and the flag at pak 0x622.
constexpr uint32_t save_block_guest = 0x80088208;
constexpr uint32_t save_block_size = 0x200;
constexpr uint32_t save_block_pak = 0x600;

std::vector<uint8_t> save_block;

// Ranges already mapped or known to move on their own, per
// lib/gga/docs/save_memory_map.md. They drown out whatever is being hunted, so
// the report counts them instead of printing them. Inclusive rdram offsets.
// The event flag bitfield is deliberately absent: it is the mapping target.
struct IgnoredRange {
    uint32_t begin;
    uint32_t end;
};

constexpr IgnoredRange ignored_rdram_ranges[] = {
    { 0x88275, 0x88275 },  // entry passes collected, area 0
    // Identified player fields only. The swizzle scatters each 10 byte block, so
    // these are not one range, and the bytes still unmapped stay visible.
    { 0x8821A, 0x8821A },  // chain pipe NPC state
    { 0x88280, 0x88280 },  // p1 lives
    { 0x88282, 0x88282 },  // chain pipe money given
    { 0x88284, 0x88287 },  // p1 money, armor, HP
    { 0x88289, 0x8828A },  // p1 held item, weapon
    { 0x8828C, 0x8828E },  // p2 armor, HP, lives
    { 0x88290, 0x88290 },  // p2 weapon
    { 0x88292, 0x88293 },  // p2 money
    { 0x88297, 0x88297 },  // p2 held item
    { 0x882A8, 0x882A9 },  // Edo access: Lost'n Town, Lost'n Road
    { 0x882AC, 0x882AF },  // Edo access: checkpoint x2, Digadig, Ringbell
    { 0x882B0, 0x882B1 },  // Edo cleared: Lost'n Road, Home of the Wiseman
    { 0x882B3, 0x882B9 },  // Edo Castle access, doors, 3 cleared, 2 Ryugu
    { 0x882BB, 0x882BB },  // Edo Castle cleared
    { 0x882BD, 0x882BF },  // Ryugu access bytes
    { 0x88343, 0x88343 },  // changes continuously, unidentified
    { 0x8836C, 0x8839A },  // position and camera floats
};

bool is_ignored(uint32_t rdram_offset) {
    for (const IgnoredRange& range : ignored_rdram_ranges) {
        if (rdram_offset >= range.begin && rdram_offset <= range.end) {
            return true;
        }
    }
    return false;
}

void save_block_snapshot(uint8_t* rdram) {
    const uint8_t* src = rdram + (save_block_guest - 0x80000000u);
    save_block.assign(src, src + save_block_size);
    fprintf(stderr, "[save] snapshot taken\n");
}

void save_block_report(uint8_t* rdram) {
    if (save_block.empty()) {
        fprintf(stderr, "[save] no snapshot, press F7 first\n");
        return;
    }

    const uint8_t* src = rdram + (save_block_guest - 0x80000000u);
    int changed = 0;
    int ignored = 0;
    for (uint32_t off = 0; off < save_block_size; off++) {
        if (src[off] != save_block[off]) {
            // rdram is byte swizzled, so a flat offset is the guest address
            // xor 3. Both are printed: the offset is what a debugger wants, the
            // guest address is what the disassembly uses.
            uint32_t rdram_offset = (save_block_guest - 0x80000000u) + off;
            if (is_ignored(rdram_offset)) {
                ignored++;
                continue;
            }
            // The pak file is in guest order, so its offset follows the guest
            // address rather than the rdram offset this loop walks.
            uint32_t guest = 0x80000000u | (rdram_offset ^ 3);
            uint32_t pak = guest - save_block_guest + save_block_pak;
            fprintf(stderr, "[save] rdram +0x%05X (guest 0x%08X, pak 0x%04X): 0x%02X -> 0x%02X\n",
                    rdram_offset, guest, pak, save_block[off], src[off]);
            changed++;
        }
    }
    fprintf(stderr, "[save] %d bytes changed (%d already mapped, hidden)\n", changed, ignored);
    save_block.assign(src, src + save_block_size);
}

void reset() {
    snapshot.clear();
    candidates.clear();
    have_snapshot = false;
    fprintf(stderr, "[search] reset\n");
}

void take_snapshot(uint8_t* rdram) {
    snapshot.assign(rdram, rdram + search_size);
}

void step(uint8_t* rdram) {
    if (!have_snapshot) {
        take_snapshot(rdram);
        candidates.clear();
        have_snapshot = true;
        fprintf(stderr, "[search] baseline taken, collect a pass then press F9\n");
        return;
    }

    std::vector<uint32_t> next;

    if (candidates.empty()) {
        for (uint32_t off = 0; off < search_size; off++) {
            if (rdram[off] == (uint8_t)(snapshot[off] + 1)) {
                next.push_back(off);
            }
        }
    }
    else {
        for (uint32_t off : candidates) {
            if (rdram[off] == (uint8_t)(snapshot[off] + 1)) {
                next.push_back(off);
            }
        }
    }

    candidates = std::move(next);
    take_snapshot(rdram);

    fprintf(stderr, "[search] %zu candidates\n", candidates.size());
    if (candidates.size() <= 32) {
        for (uint32_t off : candidates) {
            fprintf(stderr, "[search]   guest 0x%08X (host %p) = 0x%02X\n",
                    off | 0x80000000u, (void*)(rdram + off), rdram[off]);
        }
    }
}

}

void goemon64::value_search_poll(uint8_t* rdram) {
    static bool base_reported = false;
    if (!base_reported) {
        fprintf(stderr, "[search] rdram base %p, entry passes (guest 0x%08X) at host %p\n",
                (void*)rdram, entry_pass_guest,
                (void*)(rdram + (entry_pass_guest - 0x80000000u)));
        base_reported = true;
    }

    int numkeys = 0;
    const uint8_t* keys = SDL_GetKeyboardState(&numkeys);
    if (keys == nullptr || numkeys <= SDL_SCANCODE_F10) {
        return;
    }

    bool f7_down = keys[SDL_SCANCODE_F7] != 0;
    if (f7_down && !f7_was_down) {
        save_block_snapshot(rdram);
    }
    f7_was_down = f7_down;

    bool f8_down = keys[SDL_SCANCODE_F8] != 0;
    if (f8_down && !f8_was_down) {
        save_block_report(rdram);
    }
    f8_was_down = f8_down;

    bool f9_down = keys[SDL_SCANCODE_F9] != 0;
    if (f9_down && !f9_was_down) {
        step(rdram);
    }
    f9_was_down = f9_down;

    bool f10_down = keys[SDL_SCANCODE_F10] != 0;
    if (f10_down && !f10_was_down) {
        reset();
    }
    f10_was_down = f10_down;
}
