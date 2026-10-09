// Raw SI DMA and a minimal joybus. GGA's controller layer calls
// __osSiRawStartDma directly instead of osCont*, so the PIF protocol has to be
// answered somewhere. It lives here rather than in librecomp because what it
// answers is per-game, notably that a pak is always present.

#include <cstring>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/input.hpp"

#include "goemon_game.h"

void save_write_ptr(const void* in, uint32_t offset, uint32_t count);
void save_read_ptr(void* out, uint32_t offset, uint32_t count);

ultramodern::input::connected_device_info_t goemon64::get_connected_device_info(int controller_num) {
    if (controller_num == 0) {
        return ultramodern::input::connected_device_info_t {
            .connected_device = ultramodern::input::Device::Controller,
            .connected_pak = ultramodern::input::Pak::RumblePak,
        };
    }

    return ultramodern::input::connected_device_info_t {
        .connected_device = ultramodern::input::Device::None,
        .connected_pak = ultramodern::input::Pak::None,
    };
}

#define MAXCONTROLLERS 4

constexpr uint8_t JOYBUS_ERR_NO_DEVICE = 0x80;
constexpr s32 SI_DIR_WRITE = 1;  // OS_WRITE

// The pak's 32K is the front of the save buffer, so it persists through the
// existing per-game save file. The game formats it itself, which is what
// produces the ID block and inode table.
constexpr uint32_t PakSize = 32 * 1024;
constexpr uint32_t PakSaveOffset = 0;

// Above the pak's address space the accessory identifies itself (0x8000) and
// the motor is driven (0xC000).
constexpr uint32_t MotorControlAddr = 0xC000;

static uint8_t pif_ram[64];

// The command block as the game last wrote it. libultra writes a block only
// when the command changes, so an OS_READ has to re-answer the standing one.
static uint8_t pif_cmd[64];

// Joybus data CRC over a 32 byte pak block. The game verifies it.
static uint8_t pak_data_crc(const uint8_t* data) {
    uint8_t crc = 0;
    for (int i = 0; i <= 32; i++) {
        for (int bit = 7; bit >= 0; bit--) {
            uint8_t xor_val = ((crc & 0x80) != 0) ? 0x85 : 0x00;
            crc <<= 1;
            if (i < 32 && (data[i] & (1 << bit)) != 0) {
                crc |= 1;
            }
            crc ^= xor_val;
        }
    }
    return crc;
}

// `replay` is set when re-answering the standing block for an OS_READ. Hardware
// performed any write once, when the block was sent; repeating it would re-run
// pak writes 60 times a second for as long as that block stood.
static void joybus_process(bool replay) {
    int channel = 0;
    size_t i = 0;

    // Nothing else refreshes host input when the game talks to the controllers
    // this way.
    ultramodern::input::poll();

    OSContPad pads[MAXCONTROLLERS] = {};
    osContGetReadData(pads);

    bool pak_in = goemon64::get_connected_device_info(0).connected_pak != ultramodern::input::Pak::None;

    // A sequence of command blocks; the final byte is the control byte.
    while (i < sizeof(pif_ram) - 1) {
        uint8_t tx = pif_ram[i];

        if (tx == 0x00) {        // skip this channel
            channel++;
            i++;
            continue;
        }
        if (tx == 0xFE) {        // end of command list
            break;
        }
        if (tx == 0xFD || tx == 0xFF) {  // channel reset / padding
            i++;
            continue;
        }

        if (i + 1 >= sizeof(pif_ram) - 1) {
            break;
        }
        uint8_t rx = pif_ram[i + 1] & 0x3F;
        size_t cmd_off = i + 2;
        size_t res_off = cmd_off + tx;
        if (res_off + rx > sizeof(pif_ram) - 1) {
            break;
        }

        uint8_t cmd = pif_ram[cmd_off];

        if (channel != 0 || pads[0].err_no != 0) {
            pif_ram[i + 1] |= JOYBUS_ERR_NO_DEVICE;
        }
        else switch (cmd) {
            case 0x00:  // request info
            case 0xFF:  // reset and request info
                if (rx >= 3) {
                    pif_ram[res_off + 0] = 0x05;  // standard controller
                    pif_ram[res_off + 1] = 0x00;
                    pif_ram[res_off + 2] = pak_in ? 0x01 : 0x02;  // bit 0: pak inserted
                }
                break;
            case 0x01:  // read button and stick state
                if (rx >= 4) {
                    uint16_t button = (uint16_t)pads[0].button;
                    pif_ram[res_off + 0] = (uint8_t)(button >> 8);
                    pif_ram[res_off + 1] = (uint8_t)(button & 0xFF);
                    pif_ram[res_off + 2] = (uint8_t)pads[0].stick_x;
                    pif_ram[res_off + 3] = (uint8_t)pads[0].stick_y;
                }
                break;
            case 0x02: {  // read 32 bytes from the pak
                // The address bytes hold an 11 bit block address; the low 5 are
                // a check code.
                uint32_t addr = ((uint32_t)pif_ram[cmd_off + 1] << 8 | pif_ram[cmd_off + 2]) & 0xFFE0;
                if (rx >= 33) {
                    if (addr + 32 <= PakSize) {
                        save_read_ptr(&pif_ram[res_off], PakSaveOffset + addr, 32);
                    }
                    else {
                        // 0x00 leaves the accessory readable as a controller
                        // pak. Answering 0x80 identifies a rumble pak, and the
                        // game asks the accessory what it is rather than probing
                        // each region -- it cannot be both. Saving wins; rumble
                        // would need a game-side patch.
                        memset(&pif_ram[res_off], 0x00, 32);
                    }
                    pif_ram[res_off + 32] = pak_data_crc(&pif_ram[res_off]);
                }
                break;
            }
            case 0x03: {  // write 32 bytes to the pak
                uint32_t addr = ((uint32_t)pif_ram[cmd_off + 1] << 8 | pif_ram[cmd_off + 2]) & 0xFFE0;
                const uint8_t* payload = &pif_ram[cmd_off + 3];

                if (!replay) {
                    if (addr >= MotorControlAddr) {
                        ultramodern::set_rumble(channel, payload[0] != 0);
                    }
                    else if (addr + 32 <= PakSize) {
                        save_write_ptr(payload, PakSaveOffset + addr, 32);
                    }
                }
                if (rx >= 1) {
                    pif_ram[res_off] = pak_data_crc(payload);
                }
                break;
            }
            default:    // EEPROM (0x04/0x05) and anything else
                pif_ram[i + 1] |= JOYBUS_ERR_NO_DEVICE;
                break;
        }

        channel++;
        i = res_off + rx;
    }
}

extern "C" void __osSiRawStartDma_recomp(uint8_t* rdram, recomp_context* ctx) {
    // s32 __osSiRawStartDma(s32 dir, void *dramAddr)
    s32 dir = _arg<0, s32>(rdram, ctx);
    PTR(void) dram_addr = _arg<1, PTR(void)>(rdram, ctx);

    if (dir == SI_DIR_WRITE) {
        for (size_t i = 0; i < sizeof(pif_ram); i++) {
            pif_cmd[i] = MEM_B((int32_t)i, dram_addr);
        }
        memcpy(pif_ram, pif_cmd, sizeof(pif_ram));
        joybus_process(false);
    }
    else {
        memcpy(pif_ram, pif_cmd, sizeof(pif_ram));
        joybus_process(true);
        for (size_t i = 0; i < sizeof(pif_ram); i++) {
            MEM_B((int32_t)i, dram_addr) = pif_ram[i];
        }
    }

    ultramodern::send_si_message();

    _return<s32>(ctx, 0);
}
