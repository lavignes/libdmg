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
            uint8_t c;

            uint8_t b;
        };
        uint16_t bc;
    };

    union {
        struct {
            uint8_t e;

            uint8_t d;
        };
        uint16_t de;
    };

    union {
        struct {
            uint8_t l;

            uint8_t h;
        };
        uint16_t hl;
    };

    bool halted;

    uint8_t halt_flags;

    bool stopped;

    bool ime;
};

void dmg_cpu_run(DMGState *state, size_t cycles);

DMG_EXTERN_END

#endif // DMG_CPU_H