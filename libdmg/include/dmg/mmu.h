#ifndef DMG_MMU_H
#define DMG_MMU_H

#include <dmg/porting.h>

DMG_EXTERN_BEGIN

typedef struct DMGState DMGState;

typedef struct DMGMmu DMGMmu;

struct DMGMmu {
    /**
     * 0x8000-0x9FFF Video RAM (2 banks on Gameboy Color)
     */
    uint8_t vram[2][8192]; // 8KB

    /**
     * 0xA000-0xBFFF Optional cart extension RAM
     */
    uint8_t cram[8192]; // 8KB

    /**
     * 0xC000-0xCFFF Onboard Working RAM
     */
    uint8_t wram[8192]; // 8KB

    /**
     * 0xD000-0xDFFF Switchable RAM banks (on Gameboy Color)
     */
    uint8_t sram[8][8192]; // 8KB

    /**
     * 0xFE00-0xFE9F OAM Table
     */
    uint8_t oam[160];

    /**
     * 0xFF00-0xFF7F IO. Extended to 0xFFFF to hold IE Register
     */
    uint8_t io[256];

    /**
     * 0xFF80-0xFFFE HI RAM
     */
    uint8_t hram[128];
};

typedef enum DMGIOPort DMGIOPort;

enum DMGIOPort {

    /**
     * Port/Mode Registers
     */
    DMG_IO_JOYP =  0x00,
    DMG_IO_SB =    0x01,
    DMG_IO_SC =    0x02,
    DMG_IO_DIV =   0x03,
    DMG_IO_TIMA =  0x04,
    DMG_IO_TMA =   0x05,
    DMG_IO_TAC =   0x06,
    DMG_IO_KEY1 =  0x4D,
    DMG_IO_RP =    0x56,

    /**
     * Bank Control Registers
     */
    DMG_IO_VBK =   0x4F,
    DMG_IO_SVBK =  0x70,
    DMG_IO_BIOS =  0x50,

    /**
     * Interrupt Flags
     */
    DMG_IO_IF =    0x0F,
    DMG_IO_IE =    0xFF,

    /**
     * LCD Display Registers
     */
    DMG_IO_LCDC =  0x40,
    DMG_IO_STAT =  0x41,
    DMG_IO_SCY =   0x42,
    DMG_IO_SCX =   0x43,
    DMG_IO_LY =    0x44,
    DMG_IO_LYC =   0x45,
    DMG_IO_DMA =   0x46,
    DMG_IO_BGP =   0x47,
    DMG_IO_OBP0 =  0x48,
    DMG_IO_OBP1 =  0x49,
    DMG_IO_WY =    0x4A,
    DMG_IO_WX =    0x4B,
    DMG_IO_HDMA1 = 0x51,
    DMG_IO_HDMA2 = 0x52,
    DMG_IO_HDMA3 = 0x53,
    DMG_IO_HDMA4 = 0x54,
    DMG_IO_HDMA5 = 0x55,
    DMG_IO_BCPS =  0x68,
    DMG_IO_BCPD =  0x69,
    DMG_IO_OCPS =  0x6A,
    DMG_IO_OCPD =  0x6B,

    /**
     * Sound 1
     */
    DMG_IO_NR10 =  0x10,
    DMG_IO_NR11 =  0x11,
    DMG_IO_NR12 =  0x12,
    DMG_IO_NR13 =  0x13,
    DMG_IO_NR14 =  0x14,

    /**
     * Sound 2
     */
    DMG_IO_NR20 =  0x15,
    DMG_IO_NR21 =  0x16,
    DMG_IO_NR22 =  0x17,
    DMG_IO_NR23 =  0x18,
    DMG_IO_NR24 =  0x19,

    /**
     * Sound 3
     */
    DMG_IO_NR30 =  0x1A,
    DMG_IO_NR31 =  0x1B,
    DMG_IO_NR32 =  0x1C,
    DMG_IO_NR33 =  0x1D,
    DMG_IO_NR34 =  0x1E,

    /**
     * Sound 4
     */
    DMG_IO_NR40 =  0x1F,
    DMG_IO_NR41 =  0x20,
    DMG_IO_NR42 =  0x21,
    DMG_IO_NR43 =  0x22,
    DMG_IO_NR44 =  0x23,

    /**
     * Sound Control
     */
    DMG_IO_NR50 =  0x24,
    DMG_IO_NR51 =  0x25,
    DMG_IO_NR52 =  0x26,
};

uint8_t dmg_mmu_read(DMGState *state, uint16_t address);

void dmg_mmu_write(DMGState *state, uint16_t address, uint8_t byte);

DMG_EXTERN_END

#endif // DMG_MMU_H