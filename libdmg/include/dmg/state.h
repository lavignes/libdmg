#ifndef DMG_STATE_H
#define DMG_STATE_H

#include <dmg/porting.h>
#include <dmg/cpu.h>
#include <dmg/mmu.h>

DMG_EXTERN_BEGIN

typedef struct DMGState DMGState;
struct DMGState {
    uint8_t *rom;

    DMGCpu cpu;
    DMGMmu mmu;

    size_t cycles;
};

DMG_EXTERN_END

#endif // DMG_STATE_H