#include <dmg/ppu.h>
#include <dmg/mmu.h>
#include <dmg/state.h>

static DMG_INLINE uint8_t bg_palette_for_data(uint8_t data, uint8_t bgp) {
    switch (data) {
        case 0x00:
            return (uint8_t) (bgp & 0x03);
        case 0x01:
            return (uint8_t) ((bgp & 0x0C) >> 2);
        case 0x02:
            return (uint8_t) ((bgp & 0x30) >> 4);
        case 0x03:
            return (uint8_t) ((bgp & 0xC0) >> 6);
        default:
            break;
    }
    return 0;
}

static DMG_INLINE uint32_t bg_pixel_at(DMGState *state, uint8_t x, uint8_t y, uint8_t lcdc, uint8_t scx, uint8_t scy, uint8_t bgp) {
    uint16_t bg_tile_map_table = (uint16_t) ((lcdc & 0x08)? 0x9C00 : 0x9800);
    uint8_t tile_x = (x + scx) >> 3; // / 8;
    uint8_t tile_y = (y + scy) >> 3; // / 8
    uint16_t tile_idx = (tile_y << 5) + tile_x; // * 32 + tile_x
    uint8_t tile_code = dmg_mmu_read(state, bg_tile_map_table + tile_idx);
    uint16_t bg_pattern_table = (uint16_t) ((lcdc & 0x10)? (0x8000 + (tile_code * 16)) : 0x9000 + (((int8_t) tile_code) * 16));
    uint8_t char_x = (uint8_t) ((x + scx) & 0x07); // % 8
    uint8_t char_y = (uint8_t) (((y + scy) & 0x07) << 1); // % 8 * 2
    uint8_t bitlow = (uint8_t) ((dmg_mmu_read(state, bg_pattern_table + char_y) & (0x80 >> char_x)) != 0);
    uint8_t bithigh = (uint8_t) ((dmg_mmu_read(state, (uint16_t) (bg_pattern_table + char_y + 1)) & (0x80 >> char_x)) != 0);
    switch (bg_palette_for_data((bithigh << 1) | bitlow, bgp)) {
        case 0x00:
            return 0xFFFFFFFF;
        case 0x01:
            return 0xAAAAAAFF;
        case 0x02:
            return 0x555555FF;
        case 0x03:
            return 0x000000FF;
        default:
            break;
    }
    return 0x000000FF;
}

void dmg_ppu_run(DMGState *state, DMGVBlankCallback vblank, size_t cycles) {
    DMGPpu *ppu = &state->ppu;
    DMGMmu *mmu = &state->mmu;
    uint8_t lcdc = mmu->io[DMG_IO_LCDC];
    if ((lcdc & 0x80) == 0x00) {
        mmu->io[DMG_IO_LY] = 0x00;
        mmu->io[DMG_IO_STAT] &= ~0x03;
        ppu->timer = 456;
        return;
    }
    uint8_t stat = mmu->io[DMG_IO_STAT];
    uint8_t ly = mmu->io[DMG_IO_LY];
    uint8_t mode;
    if (ly >= 144) {
        mode = 0x01;
        ppu->stat_raised = false;
    } else if (ppu->timer >= 456 - 80) {
        mode = 0x02;
        ppu->stat_raised = false;
    } else if (ppu->timer >= 456 - 80 - 172) {
        mode = 0x03;
        ppu->stat_raised = false;
    } else {
        mode = 0x00;
        if (!ppu->stat_raised && (stat & 0x08)) {
            mmu->io[DMG_IO_IF] |= 0x02; // stat
            ppu->stat_raised = true;
        }
    }
    ppu->timer--;
    if (ppu->timer == 0) {
        ppu->timer += 456;
        ly++;
        if (ly == 144) {
            if (!ppu->vblank_raised) {
                mmu->io[DMG_IO_IF] |= 0x01; // vblank
                if (stat & 0x10) {
                    mmu->io[DMG_IO_IF] |= 0x02; // stat
                }
                ppu->vblank_raised = true;
            }
            vblank(state);
        } else if (ly > 153) {
            ppu->vblank_raised = false;
            ly = 0;
        }
        bool coincidence = (ly == mmu->io[DMG_IO_LYC]);
        if (coincidence && (stat & 0x40)) {
            mmu->io[DMG_IO_IF] |= 0x02;
        }
        stat = (uint8_t) ((stat & ~0x04) | coincidence);
        if (ly < 144) {
            if (lcdc & 0x01) {
                uint8_t scx = mmu->io[DMG_IO_SCX];
                uint8_t scy = mmu->io[DMG_IO_SCY];
                uint8_t bgp = mmu->io[DMG_IO_BGP];
                for (uint8_t x = 0; x < 160; x++) {
                    ppu->lcd[ly * 160 + x] = bg_pixel_at(state, x, ly, lcdc, scx, scy, bgp);
                }
            }
        }
    }
    stat = (uint8_t) ((stat & ~0x03) | mode);
    mmu->io[DMG_IO_LY] = ly;
    mmu->io[DMG_IO_STAT] = stat;
}