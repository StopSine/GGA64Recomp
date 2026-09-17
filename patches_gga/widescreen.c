#include "patch_helpers.h"

DECLARE_FUNC(float, recomp_get_target_aspect_ratio, float);

extern u8 D_800881C0[];          // graphics context; scissor at 0x14..0x1A
extern u8 *D_80108B70_5C3D90;    // the live scene, if any
extern u8 D_801736C0[];          // flag array, 30 byte entries
extern u32 D_80108C50_5C3E70[];  // static display list for the building entry fade
extern void *D_80108660_5C3880;
extern void *D_8016FCF0;

void func_800D45A0_58F7C0(void *arg0, s32 arg1, s32 arg2);
void func_800D4338_58F558(void *arg0);
void func_800D3FF0_58F210(void *arg0, void *callback, s32 arg2, s32 arg3);

#define GFX_SCISSOR_ULX (*(u16 *)&D_800881C0[0x14])
#define GFX_SCISSOR_ULY (*(u16 *)&D_800881C0[0x16])
#define GFX_SCISSOR_LRX (*(u16 *)&D_800881C0[0x18])
#define GFX_SCISSOR_LRY (*(u16 *)&D_800881C0[0x1A])

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// The game insets the scissor by 16 pixels either side, presumably for overscan.
#define INSET_ULX 16
#define INSET_LRX 304

// The fade's two G_SETSCISSOR words. A scissor packs its corners as 10.2 fixed
// point, so these are 16, 8, 304, 232 shipped and 0, 8, 320, 232 widened.
#define FADE_SCISSOR_HI 4
#define FADE_SCISSOR_LO 5

#define FADE_SCISSOR_HI_NARROW 0xED040020u
#define FADE_SCISSOR_HI_WIDE   0xED000020u
#define FADE_SCISSOR_LO_NARROW 0x004C03A0u
#define FADE_SCISSOR_LO_WIDE   0x005003A0u

static const float g_original_aspect_ratio = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;

// The last scissor this widened, so a switch back to 4:3 can be undone safely.
static u16 g_widened[4];
static int g_have_widened = 0;

static int widescreen_active(void) {
    return recomp_get_target_aspect_ratio(g_original_aspect_ratio) != g_original_aspect_ratio;
}

// The fade's scissor is constant data rather than a runtime write, so widening
// the graphics context never reaches it. Rewritten in place since the list is
// branched to directly.
static void set_fade_scissor(void) {
    u32 hi = widescreen_active() ? FADE_SCISSOR_HI_WIDE : FADE_SCISSOR_HI_NARROW;
    u32 lo = widescreen_active() ? FADE_SCISSOR_LO_WIDE : FADE_SCISSOR_LO_NARROW;

    if (D_80108C50_5C3E70[FADE_SCISSOR_HI] != hi) {
        D_80108C50_5C3E70[FADE_SCISSOR_HI] = hi;
    }

    if (D_80108C50_5C3E70[FADE_SCISSOR_LO] != lo) {
        D_80108C50_5C3E70[FADE_SCISSOR_LO] = lo;
    }
}

// RT64 only widens a projection whose scissor spans the framebuffer width, so
// the 16 pixel inset is what letterboxes the game. Restoring checks all four
// bounds, since a 0..320 scissor is legitimate on the logo and menu screens.
static void apply_scissor(void) {
    set_fade_scissor();

    if (widescreen_active()) {
        if (GFX_SCISSOR_ULX == INSET_ULX && GFX_SCISSOR_LRX == INSET_LRX) {
            GFX_SCISSOR_ULX = 0;
            GFX_SCISSOR_LRX = SCREEN_WIDTH;

            g_widened[0] = GFX_SCISSOR_ULX;
            g_widened[1] = GFX_SCISSOR_ULY;
            g_widened[2] = GFX_SCISSOR_LRX;
            g_widened[3] = GFX_SCISSOR_LRY;
            g_have_widened = 1;
        }
    }
    else if (g_have_widened) {
        if (GFX_SCISSOR_ULX == g_widened[0] && GFX_SCISSOR_ULY == g_widened[1] &&
            GFX_SCISSOR_LRX == g_widened[2] && GFX_SCISSOR_LRY == g_widened[3]) {
            GFX_SCISSOR_ULX = INSET_ULX;
            GFX_SCISSOR_LRX = INSET_LRX;
        }

        g_have_widened = 0;
    }
}

static void set_scissor(u16 ulx, u16 uly, u16 lrx, u16 lry) {
    GFX_SCISSOR_ULX = ulx;
    GFX_SCISSOR_ULY = uly;
    GFX_SCISSOR_LRX = lrx;
    GFX_SCISSOR_LRY = lry;
    apply_scissor();
}

// Re-applied every frame, immediately before func_800D45A0_58F7C0 emits the
// scissor, so an aspect ratio change mid level takes effect on the next frame
// rather than the next loadzone. That function reads neither a3 nor any stack
// argument, so forwarding three arguments reproduces the call exactly.
void widescreen_frame_hook(void *arg0, s32 arg1, s32 arg2) {
    apply_scissor();
    func_800D45A0_58F7C0(arg0, arg1, arg2);
}

// @recomp Register the hook above in place of func_800D45A0_58F7C0, which is
// too large to replace and is only ever reached as this callback.
RECOMP_PATCH void func_800D48B4_58FAD4(void) {
    func_800D4338_58F558(D_80108660_5C3880);
    func_800D3FF0_58F210(D_8016FCF0, widescreen_frame_hook, 0, 0);
}

// @recomp Widen the scissor the fade sets. The original sets the graphics
// context's scissor to one of two rectangles and sets or clears bit 1 of a flag
// the scene holds; reproduced exactly.
RECOMP_PATCH void func_800DEC40_599E60(s32 mode) {
    u8 *scene = D_80108B70_5C3D90;

    if (scene == NULL) {
        return;
    }

    u16 **flag_holder = (u16 **)(scene + 0x30);

    if (mode == 1) {
        set_scissor(INSET_ULX, 8, INSET_LRX, 198);
        **flag_holder |= 0x2;
    }
    else {
        set_scissor(INSET_ULX, 8, INSET_LRX, 232);
        **flag_holder &= ~0x2;
    }
}

// @recomp Widen the scissor level setup leaves behind. The original sets or
// clears bit 1 of an entry's first byte; reproduced exactly.
// func_800DA850_595A70 writes the scissor directly and is too large to replace,
// but it calls this immediately afterwards.
RECOMP_PATCH void func_800D8CA0_593EC0(s32 index, s32 set) {
    u8 *entry = &D_801736C0[index * 30];

    if (set) {
        entry[0] |= 0x2;
    }
    else {
        entry[0] &= ~0x2;
    }

    apply_scissor();
}
