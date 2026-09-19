#include "patch_helpers.h"

DECLARE_FUNC(float, recomp_get_target_aspect_ratio, float);
DECLARE_FUNC(void, recomp_show_message, const char* data, u32 size);

// Print each actor's name the first time one comes inside its own activation
// radius, to match these names to what is on screen. Diagnostic; set to 0 to
// silence.
#define ANNOUNCE_ACTORS 1

#if ANNOUNCE_ACTORS
#define ANNOUNCE(name, radius, d)                                  \
    do {                                                           \
        static int announced = 0;                                  \
        if (!announced && (d) < (radius)) {                        \
            announced = 1;                                         \
            recomp_show_message(name, sizeof(name) - 1);           \
        }                                                          \
    } while (0)
#else
#define ANNOUNCE(name, radius, d) ((void)0)
#endif

// Core actor and player helpers every actor overlay calls.
//
// An actor does not store a world position: it stores a distance along one of
// the level's paths, a lateral offset from that path, and the path's id.
// actor_get_world_position resolves that through path_local_to_world, which
// evaluates the path's spline and offsets perpendicular to it, and copies the
// height straight through. player_get_position is the player equivalent and
// needs no such resolution, and player_slot_occupied reports whether a player
// slot holds anyone.
void actor_get_world_position(void *actor, f32 *out_xyz);
s32 player_slot_occupied(s32 player_index);
void player_get_position(void *player, f32 *x, f32 *y, f32 *z);

extern void *player_slots[];  // player objects, indexed by player number

#define PLAYER_COUNT 4

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240

static const float g_original_aspect_ratio = (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT;

// An actor wakes when the nearest player comes within a radius held as an
// immediate in its overlay's own code -- 100.0 for the hellbat. That radius is
// a plain world distance with no projection term in it, which is why widening
// the game's field of view does not move it: a wider view simply reveals actors
// that were always inert out there, at the sides of the frame where a given
// screen position is further away than it is at the centre.
//
// Reporting a shortened distance moves the radius instead of the constant, and
// every actor overlay carries its own byte-for-byte copy of the routine that
// measures it, so each needs its own patch. Player ordering in the nearest
// player loop survives because these transforms are monotonic.
static float range_scale(void) {
    return g_original_aspect_ratio / recomp_get_target_aspect_ratio(g_original_aspect_ratio);
}

// For overlays whose only use of the distance is the activation compare.
static float shorten(float d) {
    return d * range_scale();
}

// For overlays that also use the distance for something close range, which
// must not move. Distances under the floor are reported exactly and only the
// tail is compressed, so a threshold below it behaves identically while an
// activation radius above it still extends. Kept just clear of the smallest
// real threshold found, area3kappa's 20.0.
#define NEAR_FLOOR 40.0f

static float shorten_far_only(float d) {
    if (d <= NEAR_FLOOR) {
        return d;
    }

    return NEAR_FLOOR + (d - NEAR_FLOOR) * range_scale();
}

// The measurement itself, reproduced exactly: the position fetch happens
// before the occupancy test, and an out of range player number or an empty
// slot both yield 0.0.
static float distance_to_player(void *actor, s32 player_index) {
    f32 self[3];
    f32 px, py, pz;
    f32 dx, dy, dz;

    if (player_index < 0 || player_index >= PLAYER_COUNT) {
        return 0.0f;
    }

    actor_get_world_position(actor, self);

    if (!player_slot_occupied(player_index)) {
        return 0.0f;
    }

    player_get_position(player_slots[player_index], &px, &py, &pz);

    dx = px - self[0];
    dy = py - self[1];
    dz = pz - self[2];

    return __builtin_sqrtf(dx * dx + dy * dy + dz * dz);
}

// @recomp Scale each actor's distance to a player by the aspect ratio, so its
// activation radius matches what the player can see. Overlay functions cannot
// be named -- every overlay links at 0x08000000, so an address there is
// ambiguous across all 311 of them -- hence the generated names, one per actor.
//
// Radii these move, and why two of them use the guarded form:
//
//   gyuki      file_169  100.0
//   hellbat    file_172  100.0
//   deadhand   file_173  130.0, and atan2f(20.0, distance) at two sites, so
//                        the distance is also an angle input -- guarded, which
//                        confines the angle shift to long range
//   nuppe      file_174   70.0
//   monoeye    file_176   80.0
//   area3kappa file_178  100.0 at three sites, plus a 20.0 proximity timer
//                        that must not move -- guarded

// ANNOUNCE takes the already shortened distance and each actor's own radius, so
// it fires on the same frame the game's gate passes rather than later.

RECOMP_PATCH f32 func_08000128_7DD948(void *actor, s32 player_index) {  // gyuki
    f32 d = shorten(distance_to_player(actor, player_index));
    ANNOUNCE("gyuki", 100.0f, d);
    return d;
}

RECOMP_PATCH f32 func_08000184_7DFD04(void *actor, s32 player_index) {  // hellbat
    f32 d = shorten(distance_to_player(actor, player_index));
    ANNOUNCE("hellbat", 100.0f, d);
    return d;
}

RECOMP_PATCH f32 func_080000FC_7E1F8C(void *actor, s32 player_index) {  // deadhand
    f32 d = shorten_far_only(distance_to_player(actor, player_index));
    ANNOUNCE("deadhand", 130.0f, d);
    return d;
}

RECOMP_PATCH f32 func_08000178_7E32A8(void *actor, s32 player_index) {  // nuppe
    f32 d = shorten(distance_to_player(actor, player_index));
    ANNOUNCE("nuppe", 70.0f, d);
    return d;
}

// saburo (file_175, func_08000128_7E4728) is deliberately left unpatched. Its
// 80.0 is an attack trigger, not an activation radius: it charges the player and
// explodes on reaching them, so a widened radius makes it charge from too far
// away rather than fixing anything. Both of its call sites use 80.0, so the
// guarded form cannot separate the charge from the wake-up either.

RECOMP_PATCH f32 func_08000178_7E5CF8(void *actor, s32 player_index) {  // monoeye
    f32 d = shorten(distance_to_player(actor, player_index));
    ANNOUNCE("monoeye", 80.0f, d);
    return d;
}

RECOMP_PATCH f32 func_08000128_7E90C8(void *actor, s32 player_index) {  // area3kappa
    f32 d = shorten_far_only(distance_to_player(actor, player_index));
    ANNOUNCE("area3kappa", 100.0f, d);
    return d;
}
