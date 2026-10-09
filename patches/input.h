#ifndef __INPUT_H__
#define __INPUT_H__

#include "patch_helpers.h"

DECLARE_FUNC(void, recomp_get_mouse_deltas, float* x, float* y);
DECLARE_FUNC(void, recomp_get_camera_inputs, float* x, float* y);
DECLARE_FUNC(void, recomp_set_right_analog_suppressed, s32 suppressed);

#endif
