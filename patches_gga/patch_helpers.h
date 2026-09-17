#ifndef PATCH_HELPERS_H
#define PATCH_HELPERS_H

#ifdef MIPS
#include <ultra64.h>
#else
#include "recomp.h"
#endif

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

// A function the runtime implements. On the MIPS side it is an ordinary
// declaration; on the host side the recompiler supplies the two-argument form.
#ifdef MIPS
#define DECLARE_FUNC(type, name, ...) \
        EXTERNC type name(__VA_ARGS__)
#else // MIPS
#define DECLARE_FUNC(type, name, ...) \
        EXTERNC void name(uint8_t *rdram, recomp_context *ctx)
#endif

// Marks a function as replacing the game function of the same name.
#define RECOMP_PATCH       __attribute__((section(".recomp_patch")))

#endif // PATCH_HELPERS_H
