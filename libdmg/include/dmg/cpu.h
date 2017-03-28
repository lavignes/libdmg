#ifndef DMG_CPU_H
#define DMG_CPU_H

#include <dmg/porting.h>

DMG_EXTERN_BEGIN

typedef struct DMGState DMGState;

typedef struct DMGCpu DMGCpu;
struct DMGCpu {

    uint16_t pc;

    uint16_t sp;

    union {
        struct {
            uint8_t f;

            uint8_t a;
        };
        uint16_t af;
    };

    union {
        struct {
            uint8_t b;

            uint8_t c;
        };
        uint16_t bc;
    };

    union {
        struct {
            uint8_t d;

            uint8_t e;
        };
        uint16_t de;
    };

    union {
        struct {
            uint8_t h;

            uint8_t l;
        };
        uint16_t hl;
    };

    bool halted;

    bool stopped;

    bool ime;
};

void dmg_cpu_run(DMGState *state, uint32_t cycles);

DMG_EXTERN_END

#endif // DMG_CPU_H