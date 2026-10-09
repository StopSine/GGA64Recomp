#include "patch_helpers.h"
#include "sound.h"

#include <PR/libaudio.h>

// The sound driver's per-voice volume stage, reimplemented from the disassembly
// so the user's BGM and SE sliders can scale the result at the point the driver
// already separates the two: sequence ids below 0x100 are music, everything
// above is sound effects.

typedef struct {
    /* 0x00 */ u32 sequence_id;
    /* 0x04 */ u8 pad04[0x02];
    /* 0x06 */ u8 flags;
    /* 0x07 */ u8 pad07[0x33];
    /* 0x3A */ u16 volume;
    /* 0x3C */ u8 pad3C[0x04];
    /* 0x40 */ s16 marker_volume;
    /* 0x42 */ s16 nested_marker_volume;
    /* 0x44 */ u8 pad44[0x04];
    /* 0x48 */ u8 note_velocity;
    /* 0x49 */ u8 pad49[0x01];
    /* 0x4A */ u16 envelope_volume_target;
    /* 0x4C */ u16 envelope_phase_duration;
    /* 0x4E */ u8 pad4E[0x5C];
    /* 0xAA */ s16 unk_AA;
    /* 0xAC */ u8 padAC[0x08];
    /* 0xB4 */ s16 previous_volume;
    /* 0xB6 */ u16 previous_duration;
    /* 0xB8 */ u8 padB8[0x06];
    /* 0xBE */ u8 unk_BE;
    /* 0xBF */ u8 padBF[0x09];
    /* 0xC8 */ s16 unk_C8;
    /* 0xCA */ u8 padCA[0x0E];
    /* 0xD8 */ u16 unk_D8;
} SoundPlayer;

extern SoundPlayer *D_800C362C;

extern ALSynth *D_8003A600;
extern ALVoice D_800C3468[];
extern u8 D_800C336C;

// Each pair is {fade step, scale}; the halfword at +2 is the scale.
extern u16 D_800C3374[2];
extern u16 D_800C3378[2];
extern u16 D_800C3380[4][2];
extern u16 D_800C3390[4];

extern s8 D_800C336E;
extern u8 D_800C336F;
extern s8 D_800C3370;
extern u8 D_800C3371;
extern s8 D_800C339B;
extern u8 D_800C339C;

s32 func_8000CB74_D774(s32 duration, s32 scale, s32 arg2);

#define CALLBACK_TIME 5000

#define VOLUME_IS_CHANGING (1 << 0)

RECOMP_PATCH void sound_player_set_volume(void) {
    SoundPlayer *player = D_800C362C;
    s32 volume;
    s32 duration;
    s32 sequence_id;
    s32 scale;
    s32 step;
    s32 i;

    player->flags &= ~VOLUME_IS_CHANGING;

    player = D_800C362C;

    volume = (player->note_velocity + player->marker_volume + player->nested_marker_volume) * player->volume;

    if (volume >= 0x800000) {
        volume = 0x7FFF;
    } else if (volume < 0) {
        volume = 0;
    } else {
        volume >>= 8;
    }

    duration = player->envelope_phase_duration;
    volume = (player->envelope_volume_target * volume) >> 8;

    if (player->unk_AA != 0) {
        volume = ((player->unk_AA + 0x100) * volume) >> 8;
        if (volume >= 0x8000) {
            volume = 0x7FFF;
        }
    }

    if (player->unk_C8 != 0) {
        step = (s16)(player->unk_C8 / 64);

        if (step < -0xFF) {
            volume = 0;
        } else {
            volume = ((step + 0x100) * volume) >> 8;
            if (volume >= 0x8000) {
                volume = 0x7FFF;
            }
        }
    }

    scale = player->unk_BE;
    if (scale != 0) {
        volume = (scale * volume) >> 8;
        duration = func_8000CB74_D774(duration, scale, 2);
        player = D_800C362C;
    }

    sequence_id = player->sequence_id & 0x7FFF;

    if (D_800C3378[1] != 0 && sequence_id != 0x45F) {
        volume = (D_800C3378[1] * volume) >> 16;
        duration = func_8000CB74_D774(duration, (D_800C3378[1] >> 8) & 0xFF, (s8)(4 - (D_800C3378[0] >> 8)));
        player = D_800C362C;
    }

    if ((player->sequence_id & 0x7FFF) < 0x100) {
        if (D_800C3374[1] != 0) {
            volume = (D_800C3374[1] * volume) >> 16;
            duration = func_8000CB74_D774(duration, (D_800C3374[1] >> 8) & 0xFF, (s8)(4 - (D_800C3374[0] >> 8)));
        }

        scale = D_800C339C;
        if (scale != 0) {
            volume = (scale * volume) >> 8;
            if (D_800C339B < 0) {
                duration = func_8000CB74_D774(duration, scale, 2);
            }
        }
    }

    player = D_800C362C;
    sequence_id = player->sequence_id & 0x7FFF;

    if (sequence_id < 0x100) {
        scale = D_800C336F;
        if (scale != 0) {
            volume = (scale * volume) >> 8;
            if (D_800C336E < 0) {
                duration = func_8000CB74_D774(duration, scale, 1);
                player = D_800C362C;
                sequence_id = player->sequence_id & 0x7FFF;
            }
        }

        // @recomp Let the user scale the background music.
        volume = (s32)(recomp_get_bgm_volume() * (float)volume);
    } else {
        scale = D_800C3371;
        if (scale != 0 && sequence_id != 0x203 && sequence_id != 0x205) {
            volume = (scale * volume) >> 8;
            if (D_800C3370 < 0) {
                duration = func_8000CB74_D774(duration, scale, 1);
                player = D_800C362C;
                sequence_id = player->sequence_id & 0x7FFF;
            }
        }

        // @recomp Let the user scale the sound effects.
        volume = (s32)(recomp_get_se_volume() * (float)volume);
    }

    for (i = 0; i < 4; i++) {
        if (sequence_id == D_800C3390[i]) {
            volume = (D_800C3380[i][1] * volume) >> 16;
            duration = func_8000CB74_D774(duration, D_800C3380[i][1] & 0xFF, (s8)(4 - (D_800C3380[i][0] >> 8)));
            player = D_800C362C;
            break;
        }
    }

    if (player->unk_D8 != 0) {
        volume = (player->unk_D8 * volume) >> 8;
    }

    if ((s16)volume != player->previous_volume || (u16)duration != player->previous_duration) {
        player->previous_volume = (s16)volume;
        D_800C362C->previous_duration = (u16)duration;
        alSynSetVol(D_8003A600, &D_800C3468[D_800C336C], (s16)volume, duration * CALLBACK_TIME);
    }
}
