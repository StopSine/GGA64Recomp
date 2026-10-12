#include "patch_helpers.h"

DECLARE_FUNC(s32, recomp_noclip_enabled);

// func_80194110_5F6B70 zeroes the horizontal components of `move` when its
// collision test reports the next step blocked, then calls this to apply it.
// Skipping the apply while noclip is on drops both the step and the collision
// pushback, leaving the position the host writes directly in place.
void func_80194A58_5F74B8(void *player, float *move);

RECOMP_PATCH void func_80194A58_5F74B8(void *player, float *move) {
    float *pos = (float *)((char *)player + 0x50);

    if (recomp_noclip_enabled()) {
        return;
    }

    pos[0] += move[0];
    pos[1] += move[1];
    pos[2] += move[2];
}
