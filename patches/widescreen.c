#include "patch_helpers.h"

DECLARE_FUNC(float, recomp_get_target_aspect_ratio, float);

extern u8 gfx_context[];          // graphics context; scissor at 0x14..0x1A
extern u8 *D_80108B70_5C3D90;    // the live scene, if any
extern u8 D_801736C0[];          // flag array, 30 byte entries
extern u32 D_80108C50_5C3E70[];  // static display list for the building entry fade
extern void *D_80108660_5C3880;
extern void *D_8016FCF0;

void func_800D45A0_58F7C0(void *arg0, s32 arg1, s32 arg2);
void func_800D4338_58F558(void *arg0);
void func_800D3FF0_58F210(void *arg0, void *callback, s32 arg2, s32 arg3);

#define GFX_SCISSOR_ULX (*(u16 *)&gfx_context[0x14])
#define GFX_SCISSOR_ULY (*(u16 *)&gfx_context[0x16])
#define GFX_SCISSOR_LRX (*(u16 *)&gfx_context[0x18])
#define GFX_SCISSOR_LRY (*(u16 *)&gfx_context[0x1A])

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

// The game insets its scissor by 16 pixels either side and 8 top and bottom,
// for CRT overscan. The viewport D_80108990_5C3BB0 covers the whole framebuffer
// either way, so dropping the inset reveals geometry that is already drawn.
#define INSET_ULX 16
#define INSET_ULY 8
#define INSET_LRX 304

// Two lower bounds are in use: level setup and the fade's raised state scissor
// to 198, everything else to 232. Both are dropped, which also centres the
// scissor on the viewport's centre row rather than 17 pixels above it.
#define INSET_LRY 232
#define INSET_LRY_LEVEL 198

// The fade's two G_SETSCISSOR words. A scissor packs its corners as 10.2 fixed
// point, so these are 16, 8, 304, 232 shipped and 0, 0, 320, 240 widened.
#define FADE_SCISSOR_HI 4
#define FADE_SCISSOR_LO 5

#define FADE_SCISSOR_HI_NARROW 0xED040020u
#define FADE_SCISSOR_HI_WIDE   0xED000000u
#define FADE_SCISSOR_LO_NARROW 0x004C03A0u
#define FADE_SCISSOR_LO_WIDE   0x005003C0u

static const float g_original_aspect_ratio = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;

// What this last wrote and what was there before, so a switch back to 4:3 can
// restore the exact rectangle rather than guessing at which bounds it had.
static u16 g_widened[4];
static u16 g_original[4];
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
// the horizontal inset is what letterboxes the game, and the vertical inset is
// the padding above and below. Dropping both fills the screen and leaves the
// scissor at exactly 4:3, well clear of RT64's 10% aspect detection threshold.
// Restoring compares all four bounds before putting the original back, since a
// full screen scissor is legitimate on the logo and menu screens.
static void apply_scissor(void) {
    set_fade_scissor();

    if (widescreen_active()) {
        if (GFX_SCISSOR_ULX == INSET_ULX && GFX_SCISSOR_LRX == INSET_LRX) {
            g_original[0] = GFX_SCISSOR_ULX;
            g_original[1] = GFX_SCISSOR_ULY;
            g_original[2] = GFX_SCISSOR_LRX;
            g_original[3] = GFX_SCISSOR_LRY;

            GFX_SCISSOR_ULX = 0;
            GFX_SCISSOR_LRX = SCREEN_WIDTH;

            if (GFX_SCISSOR_ULY == INSET_ULY) {
                GFX_SCISSOR_ULY = 0;
            }

            if (GFX_SCISSOR_LRY == INSET_LRY || GFX_SCISSOR_LRY == INSET_LRY_LEVEL) {
                GFX_SCISSOR_LRY = SCREEN_HEIGHT;
            }

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
            GFX_SCISSOR_ULX = g_original[0];
            GFX_SCISSOR_ULY = g_original[1];
            GFX_SCISSOR_LRX = g_original[2];
            GFX_SCISSOR_LRY = g_original[3];
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

// Extra widening beyond an exact aspect match, because the cull tests an
// item's origin rather than its extent: a piece of terrain whose origin is just
// outside a plane but whose geometry reaches into view is still dropped.
// Smaller widens further. Vertical gets a guard band too, for the same reason.
#define CULL_MARGIN_HORIZONTAL 0.65f
#define CULL_MARGIN_VERTICAL 0.85f

// @recomp Rotate a cull plane's normal into world space, widening the side
// planes so terrain is not dropped while it is still on screen.
//
// The caller builds four side planes from a single half-angle, so the cull
// frustum is square whatever the display aspect. Scaling a normal's leading
// component moves that plane's boundary outward in proportion; magnitude is
// irrelevant because the test only reads the sign of the dot product. The
// horizontal planes are the pair with no y component.
RECOMP_PATCH void func_800E0B58_59BD78(f32 *dest, f32 *m, f32 nx, f32 ny, f32 nz) {
    if (ny == 0.0f && nx != 0.0f) {
        float aspect = g_original_aspect_ratio /
                       recomp_get_target_aspect_ratio(g_original_aspect_ratio);
        nx *= aspect * CULL_MARGIN_HORIZONTAL;
    }
    else if (nx == 0.0f && ny != 0.0f) {
        ny *= CULL_MARGIN_VERTICAL;
    }

    dest[0] = m[0] * nx + m[4] * ny + m[8] * nz;
    dest[1] = m[1] * nx + m[5] * ny + m[9] * nz;
    dest[2] = m[2] * nx + m[6] * ny + m[10] * nz;
}

// RT64 draws a 2D rectangle at native size centred in the widened frame, so
// tiled 2D content stays in a 4:3 window while 3D geometry fills the screen.
// G_EX_ASPECT_STRETCH makes it scale those rects to the display aspect.
// Encodings from lib/rt64/include/rt64_extended_gbi.h; the hook word enables
// the extended opcode and must precede the first extended command.
#define RT64_HOOK_WORD0       0xE0525464u
#define RT64_HOOK_ENABLE      0x10000064u
#define RT64_EX_SETRECTASPECT 0x64000033u
#define G_EX_ASPECT_AUTO      0u
#define G_EX_ASPECT_STRETCH   1u

typedef struct {
    u32 w0;
    u32 w1;
} GfxCmd;

typedef void *(*node_draw_func)(void *dl, void *node);

extern u8 D_80172F88[];  // draw callbacks, one entry per node type, stride 0x1C

#define NODE_FLAGS(n)   (*(u16 *)((u8 *)(n) + 0x00))
#define NODE_TYPE(n)    (*(s16 *)((u8 *)(n) + 0x02))
#define NODE_CHILD(n)   (*(void **)((u8 *)(n) + 0x08))
#define NODE_SIBLING(n) (*(void **)((u8 *)(n) + 0x10))

// The tilemap class, whose drawer walks a grid clamped to 18 columns and 14
// rows of 16 pixels: the original 4:3 visible area exactly. Menu text is
// another instance of the same class, so its smaller grid separates the two.
#define TILEMAP_NODE_TYPE  12
#define NODE_COLUMNS(n)    (*(u16 *)((u8 *)(n) + 0x20))
#define NODE_PAGE_COUNT(n) (*(u16 *)((u8 *)(n) + 0x34))
#define BACKDROP_MIN_PAGES 16
#define BACKDROP_MIN_COLUMNS 18

static int is_level_backdrop(void *node) {
    return NODE_TYPE(node) == TILEMAP_NODE_TYPE &&
           NODE_PAGE_COUNT(node) >= BACKDROP_MIN_PAGES &&
           NODE_COLUMNS(node) >= BACKDROP_MIN_COLUMNS;
}

static GfxCmd *emit(GfxCmd *dl, u32 w0, u32 w1) {
    dl->w0 = w0;
    dl->w1 = w1;
    return dl + 1;
}

#define RT64_EX_SETRECTALIGN    0x64000006u
#define RT64_EX_SETSCISSORASPECT 0x64000034u
#define G_EX_ASPECT_ADJUST   2u
#define G_EX_ORIGIN_LEFT     0x000u
#define G_EX_ORIGIN_NONE     0x800u

// How far the backdrop has to reach, in 10.2 pixels. Rects are drawn at native
// size and anchored to the frame's left edge, so a wider display shows more of
// the map rather than a scaled copy of it.
#define COORD_MAX (1023 << 2)

static u32 frame_right(void) {
    float aspect = recomp_get_target_aspect_ratio(g_original_aspect_ratio);
    u32 px = (u32)((float)SCREEN_HEIGHT * aspect) << 2;

    if (px < (u32)(SCREEN_WIDTH << 2)) {
        px = (u32)(SCREEN_WIDTH << 2);
    }
    if (px > (u32)COORD_MAX) {
        px = (u32)COORD_MAX;
    }
    return px;
}

// The rects are anchored to the frame's left edge, but the scissor is still
// converted about the frame's centre, which clips them back to the original
// 4:3 window. Anchoring its left edge the same way and widening its right edge
// as a plain coordinate lets it span the frame. A right origin is deliberately
// not used: that adds the framebuffer width outright, giving a scissor twice
// the frame. The alignment is only consumed when a scissor is set, so one is
// emitted here and the frame's own is put back afterwards.
// The rects are anchored to the frame's left edge, but the scissor is still
// converted about the frame's centre and clips them back to a 4:3 window. Its
// origins move where each edge is measured from, while the offsets keep the
// stored rectangle where it was: that matters because the ratio adjustment is
// only applied when the stored scissor is within a tenth of 4:3, and a scissor
// that actually spans the frame disables it for everything in the frame, which
// is what stretched the interface as well as the backdrop. Anchoring the right
// edge to the frame's right and subtracting a frame width leaves the stored
// rectangle unchanged and the converted one spanning the frame.
// Three pieces of state, each captured per draw call, so no scissor has to be
// re-emitted: keep the rects at native size, measure them from the frame's own
// left edge rather than the centre of a 4:3 window, and let the scissor scale
// to the display so it does not clip them back to that window. The scissor's
// stored rectangle is left alone deliberately -- the ratio adjustment is only
// applied while it stays within a tenth of 4:3, and widening it disables that
// for everything in the frame, interface included.
static GfxCmd *set_backdrop_mode(GfxCmd *dl, u32 rect_aspect, u32 rect_origin,
                                 u32 scissor_aspect) {
    dl = emit(dl, RT64_HOOK_WORD0, RT64_HOOK_ENABLE);
    dl = emit(dl, RT64_EX_SETRECTASPECT, rect_aspect);
    dl = emit(dl, RT64_EX_SETRECTALIGN, rect_origin | (rect_origin << 12));
    dl = emit(dl, 0, 0);
    return emit(dl, RT64_EX_SETSCISSORASPECT, scissor_aspect);
}

// The tile grid starts at a sub-tile scroll offset, so its columns stop short
// of the frame's right edge by up to two tiles. The missing ones are the next
// columns of the map, so the drawer is run again with the scroll advanced past
// the grid, and everything except the columns that land on screen is compacted
// out of what it emits.
#define RECT_X_MASK 0xFFFu
#define RECT_X_SHIFT 12
#define TILE_FIXED (16 << 2)               // one tile, 10.2 pixels
#define GRID_COLUMNS 21                    // 19 emitted at most, plus the band
#define S_PER_FIXED 8                      // S10.5 texels per 10.2 pixel at dsdx 1.0
#define PAGE_SETTIMG 0xFD500000u           // starts a page's texture load
#define TILE_GROUP 5                       // pipesync, settile, rect, two halves

// gbi.h resolves G_ENDDL from G_IMMFIRST, which only matches the hardware
// opcode when F3DEX_GBI_2 is defined, and patches are not built with it.
#define OP_TEXRECT 0xE4u
#define OP_PIPESYNC 0xE7u
#define OP_ENDDL 0xDFu

#define RECT_XH(c) (((c)->w0 >> RECT_X_SHIFT) & RECT_X_MASK)
#define RECT_XL(c) (((c)->w1 >> RECT_X_SHIFT) & RECT_X_MASK)

#define NODE_ORIGIN_X(n) (*(u16 *)((u8 *)(n) + 0x14))
#define NODE_PAN_X(n) (*(f32 *)((u8 *)(n) + 0x18))
#define NODE_PAN_Y(n) (*(f32 *)((u8 *)(n) + 0x1C))
#define NODE_WRAP_X(n) (*(u8 *)((u8 *)(n) + 0x3A))
#define NODE_SPEED_X(n) (*(f32 *)((u8 *)(n) + 0x3C))
#define NODE_SPEED_Y(n) (*(f32 *)((u8 *)(n) + 0x40))

static int rect_wanted(GfxCmd *rect, u32 leftmost, int left_edge, u32 limit) {
    if (left_edge) {
        return RECT_XL(rect) == leftmost &&
               RECT_XH(rect) - RECT_XL(rect) == TILE_FIXED;
    }
    return RECT_XL(rect) < limit;
}

// Move the rect one tile left, clipping at the frame edge. s advances by the
// clipped width so the same texels stay under the same pixels, which is how
// the drawer itself handles a rect that starts off screen.
static void shift_rect_left(GfxCmd *rect) {
    u32 xh = RECT_XL(rect);
    u32 xl = (xh >= TILE_FIXED) ? (xh - TILE_FIXED) : 0;
    u32 s = (rect[1].w1 >> 16) & 0xFFFF;

    s += (TILE_FIXED - (xh - xl)) * S_PER_FIXED;
    rect[1].w1 = (s << 16) | (rect[1].w1 & 0xFFFF);

    rect->w0 = (rect->w0 & ~(RECT_X_MASK << RECT_X_SHIFT)) | (xh << RECT_X_SHIFT);
    rect->w1 = (rect->w1 & ~(RECT_X_MASK << RECT_X_SHIFT)) | (xl << RECT_X_SHIFT);
}

static void nudge_rect(GfxCmd *rect, u32 by) {
    u32 xl = RECT_XL(rect) + by;
    u32 xh = RECT_XH(rect) + by;

    if (xh > RECT_X_MASK) {
        return;
    }

    rect->w0 = (rect->w0 & ~(RECT_X_MASK << RECT_X_SHIFT)) | (xh << RECT_X_SHIFT);
    rect->w1 = (rect->w1 & ~(RECT_X_MASK << RECT_X_SHIFT)) | (xl << RECT_X_SHIFT);
}

// Compact a pass down to the rects that land on screen, dropping page loads
// left with nothing to draw. Output never overtakes input, so this is safe in
// place. The band is positioned through a whole-pixel field while the grid's
// columns sit on fractions of one, so each kept rect is nudged back into phase;
// translating a rect carries its texture with it, keeping the artwork
// continuous across the join.
static GfxCmd *compact_pass(GfxCmd *start, GfxCmd *end, u32 leftmost,
                            int left_edge, u32 limit, u32 x_shift) {
    GfxCmd *out = start;
    GfxCmd *cmd = start;

    while (cmd < end) {
        if (cmd->w0 == PAGE_SETTIMG) {
            GfxCmd *scan = cmd + 1;
            int used = 0;

            while (scan < end && scan->w0 != PAGE_SETTIMG) {
                if ((scan->w0 >> 24) == OP_TEXRECT &&
                    rect_wanted(scan, leftmost, left_edge, limit)) {
                    used = 1;
                }
                scan++;
            }

            if (!used) {
                cmd = scan;
                continue;
            }
        }

        // A tile is emitted as one fixed group, so it can be taken or dropped
        // whole rather than leaving its state changes behind.
        if ((cmd->w0 >> 24) == OP_PIPESYNC && cmd + 2 < end &&
            (cmd[2].w0 >> 24) == OP_TEXRECT) {
            int i;

            if (!rect_wanted(cmd + 2, leftmost, left_edge, limit)) {
                cmd += TILE_GROUP;
                continue;
            }

            if (left_edge) {
                shift_rect_left(cmd + 2);
            }
            else if (x_shift != 0) {
                nudge_rect(cmd + 2, x_shift);
            }

            for (i = 0; i < TILE_GROUP; i++) {
                out[i] = cmd[i];
            }
            out += TILE_GROUP;
            cmd += TILE_GROUP;
            continue;
        }

        if (out != cmd) {
            *out = *cmd;
        }
        out++;
        cmd++;
    }

    // The drawer leaves a provisional terminator at the pointer it returns.
    out->w0 = OP_ENDDL << 24;
    out->w1 = 0;
    return out;
}

// An extra pass builds a whole grid before being compacted, which must not
// happen past the caller's cursor: the list buffer need not have that much
// headroom, and overrunning it corrupts whatever follows. Built here instead,
// then only the wanted column is appended. Commands carry no self-references,
// so they can be copied out; the palette stays where the drawer allocated it.
#define SCRATCH_CMDS 8192
#define GRID_ROWS 15
#define PAGE_CMDS 8
#define PASS_OVERHEAD 64
#define MAX_EDGE_PASSES 3

static GfxCmd g_pass_scratch[SCRATCH_CMDS];

static void *run_edge_pass(void *dl, void *node, node_draw_func draw,
                           u32 leftmost, int left_edge, u32 limit, u32 x_shift) {
    GfxCmd *pass_end = (GfxCmd *)draw(g_pass_scratch, node);
    GfxCmd *kept_end = compact_pass(g_pass_scratch, pass_end, leftmost, left_edge,
                                    limit, x_shift);
    GfxCmd *out = (GfxCmd *)dl;
    GfxCmd *cmd;

    for (cmd = g_pass_scratch; cmd < kept_end; cmd++) {
        *out = *cmd;
        out++;
    }

    return out;
}

// Which map column the grid starts at, as the drawer derives it.
static s32 tile_index(f32 pan) {
    if (pan >= 0.0f) {
        return (s32)(pan / 16.0f);
    }
    return (s32)((pan + 1.0f) / 16.0f) - 1;
}

// Which column the band should show. When the map wraps, the drawer's own
// entry wrap normalises the scroll, so asking for a column past the end is
// correct and lands back at the start; a single wrap is always enough because
// the offset is under one map width. Without wrapping there is no such column,
// so the nearest real one is repeated rather than leaving the band blank.
static s32 band_column(void *node, s32 want, s32 columns) {
    if (NODE_WRAP_X(node)) {
        return want;
    }
    if (want > columns - 1) {
        return columns - 1;
    }
    if (want < 0) {
        return 0;
    }
    return want;
}

static int pass_fits(void *node) {
    u32 needed = PASS_OVERHEAD + NODE_PAGE_COUNT(node) * PAGE_CMDS +
                 GRID_COLUMNS * GRID_ROWS * TILE_GROUP;
    return needed <= SCRATCH_CMDS;
}

static void *draw_backdrop_edges(void *dl, void *node, node_draw_func draw,
                                 GfxCmd *grid, GfxCmd *grid_end) {
    u32 leftmost = RECT_X_MASK;
    u32 lowest_xh = RECT_X_MASK;
    u32 rightmost = 0;
    GfxCmd *cmd;
    f32 pan_x, pan_y, speed_x, speed_y;
    u16 origin_x;
    s32 columns, first, drawn;
    u32 limit;
    int passes = 0;

    // Taken from what the grid emitted rather than assumed: it draws 18 or 19
    // columns depending on whether the scroll sits on a tile boundary. A column
    // straddling the frame's left edge is dropped rather than clipped, so
    // depending on the scroll offset the grid can start a fraction of a tile in
    // and the left side needs a column of its own.
    for (cmd = grid; cmd < grid_end; cmd++) {
        if ((cmd->w0 >> 24) != OP_TEXRECT) {
            continue;
        }
        if (RECT_XL(cmd) < leftmost) {
            leftmost = RECT_XL(cmd);
        }
        if (RECT_XH(cmd) > rightmost) {
            rightmost = RECT_XH(cmd);
        }
        if (RECT_XH(cmd) < lowest_xh) {
            lowest_xh = RECT_XH(cmd);
        }
    }

    if (leftmost == RECT_X_MASK || !pass_fits(node)) {
        return dl;
    }

    pan_x = NODE_PAN_X(node);
    pan_y = NODE_PAN_Y(node);
    speed_x = NODE_SPEED_X(node);
    speed_y = NODE_SPEED_Y(node);
    origin_x = NODE_ORIGIN_X(node);

    // The drawer scrolls the pan on entry, so hold it still for these passes.
    NODE_SPEED_X(node) = 0.0f;
    NODE_SPEED_Y(node) = 0.0f;

    columns = NODE_COLUMNS(node);
    first = tile_index(pan_x);
    // Counted from the right edges: those step by a whole tile even when the
    // first column's left edge was clamped to the frame, so this is the real
    // column count and the column past the grid is first + drawn.
    drawn = (s32)((rightmost - lowest_xh) / TILE_FIXED) + 1;

    if (drawn <= 0) {
        return dl;
    }

    limit = frame_right();

    // Fill in the column the grid dropped for straddling the frame's left edge.
    if (leftmost > 0) {
        s32 want = band_column(node, first - 1, columns);
        NODE_PAN_X(node) = pan_x + (f32)((want - first) * 16);
        dl = run_edge_pass(dl, node, draw, leftmost, 1, limit, 0);
    }

    // Rects are drawn at native size, so the grid covers only the original 4:3
    // worth of a wider frame and the rest is filled with real columns. A pass
    // yields up to 19 of them, so one is normally enough; the loop covers
    // aspects needing more, and stops as soon as a pass adds nothing.
    while (rightmost < limit && passes < MAX_EDGE_PASSES) {
        GfxCmd *appended = (GfxCmd *)dl;
        s32 want = band_column(node, first + drawn, columns);
        u32 edge = rightmost;

        NODE_PAN_X(node) = pan_x + (f32)((want - first) * 16);
        // The override sets the first column's right edge, so subtract a tile
        // to put that column's left edge against the grid's right edge.
        NODE_ORIGIN_X(node) = (u16)(rightmost / 4 - 16);
        dl = run_edge_pass(dl, node, draw, leftmost, 0, limit, rightmost & 3);

        for (cmd = appended; cmd < (GfxCmd *)dl; cmd++) {
            if ((cmd->w0 >> 24) == OP_TEXRECT && RECT_XH(cmd) > edge) {
                edge = RECT_XH(cmd);
            }
        }

        if (edge <= rightmost) {
            break;
        }

        drawn += (s32)((edge - rightmost) / TILE_FIXED);
        rightmost = edge;
        passes++;
    }

    NODE_ORIGIN_X(node) = origin_x;
    NODE_PAN_X(node) = pan_x;
    NODE_PAN_Y(node) = pan_y;
    NODE_SPEED_X(node) = speed_x;
    NODE_SPEED_Y(node) = speed_y;
    return dl;
}

// @recomp Bracket the backdrop's rects with the stretch hint. The original
// calls the type's draw callback when bit 1 of the node's flags is set, then
// walks the children, threading the display list pointer through; reproduced
// exactly apart from the bracketing.
RECOMP_PATCH void *func_800D48F4_58FB14(void *dl, void *node) {
    void *child;

    if (NODE_FLAGS(node) & 0x2) {
        node_draw_func draw =
            *(node_draw_func *)&D_80172F88[NODE_TYPE(node) * 0x1C];

        if (draw != NULL) {
            int stretch = is_level_backdrop(node) && widescreen_active();

            GfxCmd *grid;

            if (stretch) {
                dl = set_backdrop_mode((GfxCmd *)dl, G_EX_ASPECT_ADJUST,
                                       G_EX_ORIGIN_LEFT, G_EX_ASPECT_STRETCH);
            }

            grid = (GfxCmd *)dl;
            dl = draw(dl, node);

            if (stretch) {
                dl = draw_backdrop_edges(dl, node, draw, grid, (GfxCmd *)dl);
                dl = set_backdrop_mode((GfxCmd *)dl, G_EX_ASPECT_AUTO,
                                       G_EX_ORIGIN_NONE, G_EX_ASPECT_AUTO);
            }
        }
    }

    for (child = NODE_CHILD(node); child != NULL; child = NODE_SIBLING(child)) {
        dl = func_800D48F4_58FB14(dl, child);
    }

    return dl;
}

// @recomp Widen the scissor the fade sets. The original sets the graphics
// context's scissor to one of two rectangles and sets or clears bit 1 of a flag
// the scene holds; reproduced exactly.
RECOMP_PATCH void scene_apply_scissor(s32 mode) {
    u8 *scene = D_80108B70_5C3D90;

    if (scene == NULL) {
        return;
    }

    u16 **flag_holder = (u16 **)(scene + 0x30);

    if (mode == 1) {
        set_scissor(INSET_ULX, INSET_ULY, INSET_LRX, INSET_LRY_LEVEL);
        **flag_holder |= 0x2;
    }
    else {
        set_scissor(INSET_ULX, INSET_ULY, INSET_LRX, INSET_LRY);
        **flag_holder &= ~0x2;
    }
}

// @recomp Widen the scissor level setup leaves behind. The original sets or
// clears bit 1 of an entry's first byte; reproduced exactly.
// func_800DA850_595A70 writes the scissor directly and is too large to replace,
// but it calls this immediately afterwards.
RECOMP_PATCH void flag_entry_set_bit1(s32 index, s32 set) {
    u8 *entry = &D_801736C0[index * 30];

    if (set) {
        entry[0] |= 0x2;
    }
    else {
        entry[0] &= ~0x2;
    }

    apply_scissor();
}
