#include <cmath>

#include "recomp.h"
#include "librecomp/overlays.hpp"
#include "librecomp/addresses.hpp"
#include "goemon_config.h"
#include "recomp_input.h"
#include "recomp_ui.h"
#include "goemon_render.h"
#include "goemon_sound.h"
#include "librecomp/helpers.hpp"
#include "../patches/input.h"
#include "../patches/graphics.h"
#include "../patches/sound.h"
#include "ultramodern/ultramodern.hpp"
#include "ultramodern/config.hpp"

extern "C" void recomp_update_inputs(uint8_t* rdram, recomp_context* ctx) {
    recomp::poll_inputs();
}

extern "C" void recomp_puts(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) cur_str = _arg<0, PTR(char)>(rdram, ctx);
    u32 length = _arg<1, u32>(rdram, ctx);

    for (u32 i = 0; i < length; i++) {
        fputc(MEM_B(i, (gpr)cur_str), stdout);
    }
}

extern "C" void recomp_exit(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::quit();
}

// Show a message in the on-screen feed. Same arguments as recomp_puts, since
// a patch has no way to measure a string.
// Walk the display object tree and report each node's flags, type and draw
// callback. Bit 1 of the flags is what scenegraph_draw_node tests, so a node
// that is present but not drawn shows up here with that bit clear.
// A guest pointer must be sign-extended and in mapped rdram before MEM_* reads
// it; the macros neither mask nor bounds-check, so anything else faults.
static bool plausible_guest_ptr(int32_t addr) {
    uint32_t a = (uint32_t)addr;
    return a >= 0x80000000u && a < 0x80000000u + (uint32_t)recomp::mem_size;
}

static void dump_scene_node(uint8_t* rdram, int32_t node, int depth, int& budget) {
    while (plausible_guest_ptr(node) && budget > 0) {
        budget--;

        uint32_t header = MEM_W(0x0, (gpr)node);
        uint16_t flags = (uint16_t)(header >> 16);
        int16_t type = (int16_t)(header & 0xFFFF);
        int32_t child = (int32_t)MEM_W(0x8, (gpr)node);
        uint32_t callback = 0;

        if (type >= 0 && type < 64) {
            int32_t slot = (int32_t)(0x80172F88 + type * 0x1C);
            if (plausible_guest_ptr(slot)) {
                callback = MEM_W(0x0, (gpr)slot);
            }
        }

        fprintf(stdout, "[graph] %*snode 0x%08X flags %04X%s type %d cb 0x%08X\n",
                depth * 2, "", (uint32_t)node, flags,
                (flags & 0x2) ? " DRAWN" : "      ", type, callback);

        if (child != 0 && depth < 8) {
            dump_scene_node(rdram, child, depth + 1, budget);
        }

        node = (int32_t)MEM_W(0x10, (gpr)node);
    }
}

// Dumps once each time the F9 overlay-load logging is newly switched on, so
// toggling it off and on again either side of an event gives two dumps to diff.
extern "C" void recomp_dump_scene_graph(uint8_t* rdram, recomp_context* ctx) {
    static bool armed = false;
    bool enabled = recomp::overlays::overlay_load_logging_enabled();

    if (!enabled) {
        armed = false;
        return;
    }

    if (armed) {
        return;
    }

    armed = true;

    int32_t root_holder = (int32_t)0x80172F00;
    if (!plausible_guest_ptr(root_holder)) {
        return;
    }

    int32_t root = (int32_t)MEM_W(0x0, (gpr)root_holder);
    int budget = 400;

    fprintf(stdout, "[graph] ---- root 0x%08X ----\n", (uint32_t)root);
    dump_scene_node(rdram, root, 0, budget);
    fprintf(stdout, "[graph] ---- %d nodes ----\n", 400 - budget);
}

extern "C" void recomp_show_message(uint8_t* rdram, recomp_context* ctx) {
    PTR(char) cur_str = _arg<0, PTR(char)>(rdram, ctx);
    u32 length = _arg<1, u32>(rdram, ctx);

    std::string text;
    text.reserve(length);

    for (u32 i = 0; i < length; i++) {
        text.push_back(MEM_B(i, (gpr)cur_str));
    }

    // Mirrored to stdout so a redirected run records what was shown, which is
    // the only way to tell a message that never fired from one that fired and
    // did not render.
    fprintf(stdout, "[message] %s\n", text.c_str());

    recompui::show_game_message(text);
}

extern "C" void recomp_get_gyro_deltas(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    recomp::get_gyro_deltas(x_out, y_out);
}

extern "C" void recomp_get_mouse_deltas(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    recomp::get_mouse_deltas(x_out, y_out);
}

extern "C" void recomp_powf(uint8_t* rdram, recomp_context* ctx) {
    float a = _arg<0, float>(rdram, ctx);
    float b = ctx->f14.fl; //_arg<1, float>(rdram, ctx);

    _return(ctx, std::pow(a, b));
}

extern "C" void recomp_get_target_framerate(uint8_t* rdram, recomp_context* ctx) {
    int frame_divisor = _arg<0, u32>(rdram, ctx);

    _return(ctx, ultramodern::get_target_framerate(60 / frame_divisor));
}

extern "C" void recomp_get_window_resolution(uint8_t* rdram, recomp_context* ctx) {
    int width, height;
    recompui::get_window_size(width, height);

    gpr width_out = _arg<0, PTR(u32)>(rdram, ctx);
    gpr height_out = _arg<1, PTR(u32)>(rdram, ctx);

    MEM_W(0, width_out) = (u32)width;
    MEM_W(0, height_out) = (u32)height;
}

extern "C" void recomp_get_target_aspect_ratio(uint8_t* rdram, recomp_context* ctx) {
    ultramodern::renderer::GraphicsConfig graphics_config = ultramodern::renderer::get_graphics_config();
    float original = _arg<0, float>(rdram, ctx);
    int width, height;
    recompui::get_window_size(width, height);

    switch (graphics_config.ar_option) {
        case ultramodern::renderer::AspectRatio::Original:
        default:
            _return(ctx, original);
            return;
        case ultramodern::renderer::AspectRatio::Expand:
            _return(ctx, std::max(static_cast<float>(width) / height, original));
            return;
    }
}

extern "C" void recomp_get_targeting_mode(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<int>(goemon64::get_targeting_mode()));
}

extern "C" void recomp_get_bgm_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, goemon64::get_bgm_volume() / 100.0f);
}

extern "C" void recomp_get_se_volume(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, goemon64::get_se_volume() / 100.0f);
}

extern "C" void recomp_time_us(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<u32>(std::chrono::duration_cast<std::chrono::microseconds>(ultramodern::time_since_start()).count()));
}

extern "C" void recomp_get_autosave_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, static_cast<s32>(goemon64::get_autosave_mode() == goemon64::AutosaveMode::On));
}

extern "C" void recomp_load_overlays(uint8_t * rdram, recomp_context * ctx) {
    u32 rom = _arg<0, u32>(rdram, ctx);
    PTR(void) ram = _arg<1, PTR(void)>(rdram, ctx);
    u32 size = _arg<2, u32>(rdram, ctx);

    load_overlays(rom, ram, size);
}

extern "C" void recomp_unload_overlays(uint8_t *rdram, recomp_context *ctx) {
    PTR(void) vram_addr = _arg<0, PTR(void)>(rdram, ctx);
    u32 size = _arg<1, u32>(rdram, ctx);

    unload_overlays(vram_addr, size);
}

extern "C" void recomp_high_precision_fb_enabled(uint8_t * rdram, recomp_context * ctx) {
    _return(ctx, static_cast<s32>(goemon64::renderer::RT64HighPrecisionFBEnabled()));
}

extern "C" void recomp_get_resolution_scale(uint8_t* rdram, recomp_context* ctx) {
    _return(ctx, ultramodern::get_resolution_scale());
}

extern "C" void recomp_get_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    goemon64::CameraInvertMode mode = goemon64::get_camera_invert_mode();

    *x_out = (mode == goemon64::CameraInvertMode::InvertX || mode == goemon64::CameraInvertMode::InvertBoth);
    *y_out = (mode == goemon64::CameraInvertMode::InvertY || mode == goemon64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_analog_inverted_axes(uint8_t* rdram, recomp_context* ctx) {
    s32* x_out = _arg<0, s32*>(rdram, ctx);
    s32* y_out = _arg<1, s32*>(rdram, ctx);

    goemon64::CameraInvertMode mode = goemon64::get_analog_camera_invert_mode();

    *x_out = (mode == goemon64::CameraInvertMode::InvertX || mode == goemon64::CameraInvertMode::InvertBoth);
    *y_out = (mode == goemon64::CameraInvertMode::InvertY || mode == goemon64::CameraInvertMode::InvertBoth);
}

extern "C" void recomp_get_analog_cam_enabled(uint8_t* rdram, recomp_context* ctx) {
    _return<s32>(ctx, goemon64::get_analog_cam_mode() == goemon64::AnalogCamMode::On);
}

extern "C" void recomp_get_camera_inputs(uint8_t* rdram, recomp_context* ctx) {
    float* x_out = _arg<0, float*>(rdram, ctx);
    float* y_out = _arg<1, float*>(rdram, ctx);

    // TODO expose this in the menu
    constexpr float radial_deadzone = 0.05f;

    float x, y;

    recomp::get_right_analog(&x, &y);

    float magnitude = sqrtf(x * x + y * y);

    if (magnitude < radial_deadzone) {
        *x_out = 0.0f;
        *y_out = 0.0f;
    }
    else {
        float x_normalized = x / magnitude;
        float y_normalized = y / magnitude;

        *x_out = x_normalized * ((magnitude - radial_deadzone) / (1 - radial_deadzone));
        *y_out = y_normalized * ((magnitude - radial_deadzone) / (1 - radial_deadzone));
    }
}

extern "C" void recomp_set_right_analog_suppressed(uint8_t* rdram, recomp_context* ctx) {
    s32 suppressed = _arg<0, s32>(rdram, ctx);

    recomp::set_right_analog_suppressed(suppressed);
}
