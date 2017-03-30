#ifndef DMG_PPU_H
#define DMG_PPU_H

#include <dmg/porting.h>

DMG_EXTERN_BEGIN

typedef struct DMGState DMGState;

typedef struct DMGPpu DMGPpu;

struct DMGPpu {
    bool stat_raised;
    bool vblank_raised;
    int32_t timer;
    uint32_t lcd[23040];
};

typedef void (*DMGVBlankCallback)(struct DMGState *state);

void dmg_ppu_run(DMGState *state, DMGVBlankCallback vblank, size_t cycles);

DMG_EXTERN_END

#endif // DMG_PPU_H