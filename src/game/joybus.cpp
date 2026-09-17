// Raw SI DMA and a minimal joybus, for the game's own controller layer.
//
// librecomp implements the osCont* entry points, which is enough for a game
// whose libultra controller calls are named. Goemon's Great Adventure has no
// decompilation and its controller layer reaches __osSiRawStartDma instead,
// which on hardware programs the SI registers at 0xA4800000; MMIO is not
// modelled, so the recompiled body faults. Answering the PIF protocol here
// keeps the game on the runtime's input state.
//
// This lives in the game rather than in librecomp because what it answers is a
// per-game decision -- notably that a pak is always present -- and because
// everything it needs is public runtime API.

#include <cstdio>
#include <cstring>

#include "recomp.h"
#include "librecomp/helpers.hpp"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/input.hpp"

#include "recomp_input.h"

#define MAXCONTROLLERS 4

static uint8_t pif_ram[64];

// The command block as the game last wrote it, before any response bytes were
// laid over it. libultra only writes the block when the command changes:
// osContStartReadData issues the OS_WRITE once and every later poll is a bare
// OS_READ, so a read has to re-answer the standing command rather than hand
// back the previous frame's answers.
static uint8_t pif_cmd[64];

// Joybus result flags, ORed into a command block's rx length byte.
constexpr uint8_t JOYBUS_ERR_NO_DEVICE = 0x80;

// libultra's direction values; the runtime headers do not define them and
// pi.cpp compares against the literals too.
[[maybe_unused]] constexpr s32 SI_DIR_READ = 0;   // OS_READ
constexpr s32 SI_DIR_WRITE = 1;  // OS_WRITE

// Joybus data CRC over a 32 byte pak block, as the controller computes it.
// The game verifies this, so a response with the wrong value is rejected.
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

// The pak's 32K of storage is the front of the runtime's save buffer, so the
// contents persist through the existing per-game save file rather than needing
// their own. The game formats the pak itself, which is what produces the ID
// block and inode table -- writing those here by hand would mean reproducing
// libultra's checksums exactly, and the game's own formatter cannot disagree
// with the game's own validator.
//
// Only controller 0 is backed; the other ports report no pak, so nothing else
// reaches this storage.
void save_write_ptr(const void* in, uint32_t offset, uint32_t count);
void save_read_ptr(void* out, uint32_t offset, uint32_t count);

constexpr uint32_t PakSize = 32 * 1024;
constexpr uint32_t PakSaveOffset = 0;

// The pak's contents, mirrored in memory. The game reads the pak far more often
// than it writes it, and every read through the save buffer takes that buffer's
// lock, so reads are served from here and writes go through to both. The mirror
// is filled on first use, once the runtime has loaded the save file.
static uint8_t pak_mirror[PakSize];
static bool pak_mirror_loaded = false;

static void pak_mirror_load() {
    if (!pak_mirror_loaded) {
        save_read_ptr(pak_mirror, PakSaveOffset, PakSize);
        pak_mirror_loaded = true;
    }
}
// Reads and writes above the storage area identify the accessory and drive the
// motor; they are not part of the pak's address space.
constexpr uint32_t AccessoryProbeAddr = 0x8000;
constexpr uint32_t MotorControlAddr = 0xC000;

// Whether a pak is in the given port. Games on the osCont* path get this from
// osContInit; a game that reads joybus directly has to be told the same thing,
// and it is answered from configuration rather than from a probe having already
// happened, so the pak reads as inserted from the first query onwards and the
// game never asks for one.
static bool pak_present(int channel) {
    return recomp::get_connected_device_info(channel).connected_pak != ultramodern::input::Pak::None;
}

// `replay` is set when re-answering the standing command block for an OS_READ
// rather than executing a freshly written one. The hardware performed any write
// once, when the block was sent; reading PIF RAM back only returns the stored
// response. Repeating the writes is both wrong and ruinous for performance,
// because the game polls buttons every frame with a bare OS_READ, so a pak
// write would be re-executed -- and the save file re-flushed -- 60 times a
// second for as long as that block stood.
static void joybus_process(bool replay) {
    int channel = 0;
    size_t i = 0;

    // Refresh the host's input first: nothing else does when the game talks to
    // the controllers this way.
    ultramodern::input::poll();

    OSContPad pads[MAXCONTROLLERS] = {};
    osContGetReadData(pads);

    // The buffer holds a sequence of command blocks; the final byte is the
    // control byte rather than command data.
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
        bool present = channel == 0 && channel < MAXCONTROLLERS && pads[channel].err_no == 0;

        if (!present) {
            pif_ram[i + 1] |= JOYBUS_ERR_NO_DEVICE;
        }
        else switch (cmd) {
            case 0x00:  // request info
            case 0xFF:  // reset and request info
                if (rx >= 3) {
                    // Type 0x0500 is a standard controller; no pak attached.
                    pif_ram[res_off + 0] = 0x05;
                    pif_ram[res_off + 1] = 0x00;
                    // Status bit 0 is "pak inserted"; 0x02 is what a
                    // controller reports when its slot is empty.
                    pif_ram[res_off + 2] = pak_present(channel) ? 0x01 : 0x02;
                }
                break;
            case 0x01:  // read button and stick state
                if (rx >= 4) {
                    uint16_t button = (uint16_t)pads[channel].button;
                    pif_ram[res_off + 0] = (uint8_t)(button >> 8);
                    pif_ram[res_off + 1] = (uint8_t)(button & 0xFF);
                    pif_ram[res_off + 2] = (uint8_t)pads[channel].stick_x;
                    pif_ram[res_off + 3] = (uint8_t)pads[channel].stick_y;
                }
                break;
            case 0x02: {  // read 32 bytes from the pak
                // The two address bytes hold an 11 bit block address in their
                // top bits; the low 5 bits are a check code.
                uint32_t addr = ((uint32_t)pif_ram[cmd_off + 1] << 8 | pif_ram[cmd_off + 2]) & 0xFFE0;
                if (rx >= 33) {
                    // One read for the whole block: save_read_ptr takes the
                    // save buffer's lock, and the game reads the pak often
                    // enough that doing it per byte is 32 times the locking for
                    // the same bytes.
                    if (addr < AccessoryProbeAddr && channel == 0 && addr + 32 <= PakSize) {
                        pak_mirror_load();
                        memcpy(&pif_ram[res_off], &pak_mirror[addr], 32);
                    }
                    else for (int b = 0; b < 32; b++) {
                        uint8_t value;
                        if (addr >= AccessoryProbeAddr) {
                            // 0x80 here is how a rumble pak identifies itself,
                            // and 0x00 is what leaves the accessory readable as
                            // a controller pak. It has to be one or the other:
                            // answering 0x80 while still serving the filesystem
                            // below does not work, because the game asks the
                            // accessory what it is rather than testing each
                            // region -- it drove the motor probe but then still
                            // asked for a rumble pak to be inserted. Real
                            // hardware forces the same choice. Saving wins;
                            // rumble would need a game-side patch to decouple
                            // the two.
                            value = 0x00;
                        }
                        else if (channel == 0 && addr + b < PakSize) {
                            save_read_ptr(&value, PakSaveOffset + addr + b, 1);
                        }
                        else {
                            value = 0x00;
                        }
                        pif_ram[res_off + b] = value;
                    }
                    pif_ram[res_off + 32] = pak_data_crc(&pif_ram[res_off]);
                }
                break;
            }
            case 0x03: {  // write 32 bytes to the pak
                uint32_t addr = ((uint32_t)pif_ram[cmd_off + 1] << 8 | pif_ram[cmd_off + 2]) & 0xFFE0;
                const uint8_t* payload = &pif_ram[cmd_off + 3];
                if (replay) {
                    // Already performed when the block was written. Fall
                    // through to the CRC so the response still reads back.
                }
                else if (addr >= MotorControlAddr) {
                    // Honour the motor even though the accessory reports as a
                    // controller pak. A real console cannot have both, so the
                    // game may never send this; costing nothing to accept it.
                    ultramodern::set_rumble(channel, payload[0] != 0);
                }
                else if (addr >= AccessoryProbeAddr) {
                    // Accessory probe. Nothing to record: the read back always
                    // identifies a controller pak.
                }
                else if (channel == 0 && addr + 32 <= PakSize) {
                    pak_mirror_load();
                    if (memcmp(&pak_mirror[addr], payload, 32) != 0) {
                        // Only touch the save buffer when the bytes differ. The
                        // game rewrites unchanged blocks, and each write wakes
                        // the saving thread.
                        memcpy(&pak_mirror[addr], payload, 32);
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
        // RDRAM to PIF: take the command block, keep a pristine copy of it, and
        // answer it.
        for (size_t i = 0; i < sizeof(pif_ram); i++) {
            pif_cmd[i] = MEM_B((int32_t)i, dram_addr);
        }
        memcpy(pif_ram, pif_cmd, sizeof(pif_ram));
        joybus_process(false);
    }
    else {
        // PIF to RDRAM: re-run the standing command so the answers are current,
        // then hand them back. On hardware the SI read is what latches the
        // controller state, and the poll loop never rewrites the block.
        memcpy(pif_ram, pif_cmd, sizeof(pif_ram));
        joybus_process(true);
        for (size_t i = 0; i < sizeof(pif_ram); i++) {
            MEM_B((int32_t)i, dram_addr) = pif_ram[i];
        }
    }

    // Hardware raises the SI interrupt on completion and the caller waits on
    // that event, exactly as the PI raw path does.
    ultramodern::send_si_message();

    _return<s32>(ctx, 0);
}
