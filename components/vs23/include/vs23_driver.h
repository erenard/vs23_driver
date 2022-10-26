#include "driver/spi_master.h"

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_COLORS

#define VS23_IC0_DISABLED (1<<0)
#define VS23_IC1_DISABLED (1<<1)
#define VS23_IC2_DISABLED (1<<2)
#define VS23_IC3_DISABLED (1<<3)

#define VS23_VIDEO_CONTROL1_DIRECT_DAC_ENABLED (1<<15)
#define VS23_VIDEO_CONTROL1_UV_FROM_TABLE      (1<<14)
#define VS23_VIDEO_CONTROL1_SELECT_PLL_CLOCK   (1<<13)
#define VS23_VIDEO_CONTROL1_PLL_ENABLED        (1<<12)
#define VS23_VIDEO_CONTROL1_SKIP_UV_CYCLE      (1<<1)

#define VS23_VIDEO_CONTROL2_VIDEO_ENABLED      (1<<15)
#define VS23_VIDEO_CONTROL2_PAL_MODE           (1<<14)

#define XTAL_MHZ 4.43361875
/// PLL frequency
#define PLL_MHZ (XTAL_MHZ * 8.0)

/// Define PAL video timing constants
/// PAL short sync duration is 2.35 us
#define SHORT_SYNC_US 2.35
/// For the start of the line, the first 10 extra PLLCLK sync (0) cycles
/// are subtracted.
#define SHORT_SYNC ((uint16_t)(SHORT_SYNC_US * XTAL_MHZ-10.0/8.0))
/// For the middle of the line the whole duration of sync pulse is used.
#define SHORT_SYNC_M ((uint16_t)(SHORT_SYNC_US * XTAL_MHZ))
/// PAL long sync duration is 27.3 us
#define LONG_SYNC_US 27.3
#define LONG_SYNC ((uint16_t)(LONG_SYNC_US * XTAL_MHZ))
#define LONG_SYNC_M ((uint16_t)(LONG_SYNC_US * XTAL_MHZ))
/// Normal visible picture line sync length is 4.7 us
#define SYNC_US 4.7
#define SYNC ((uint16_t)(SYNC_US*XTAL_MHZ-10.0/8.0))
/// Color burst starts at 5.6 us
#define BURST_US 5.6
#define BURST ((uint16_t)(BURST_US*XTAL_MHZ-10.0/8.0))
/// Color burst duration is 2.25 us
#define BURST_DURATION_US 2.25
#define BURST_DURATION ((uint16_t)(BURST_DURATION_US*XTAL_MHZ))
/// PAL sync to blanking end time is 10.5 us
#define BLANK_END_US 10.5
#define BLANK_END ((uint16_t)(BLANK_END_US*XTAL_MHZ-10.0/8.0))
/// Front porch starts at the end of the line, at 62.5us 
#define FRONT_PORCH_US 62.5
#define FRONT_PORCH ((uint16_t)(FRONT_PORCH_US*XTAL_MHZ-10.0/8.0))

#define LINE_LENGTH_US 64.0

/// 10 first pllclks, which are not in the counters are decremented here
#define LINE_LENGTH_PLL ((uint16_t)((LINE_LENGTH_US * PLL_MHZ)+0.5-10))
/// 10 first pllclks, which are not in the counters are decremented here
#define LINE_LENGTH ((uint16_t)((LINE_LENGTH_US * XTAL_MHZ)+0.5-10.0/8.0))
#define LINE_HALF_LENGTH ((uint16_t)((LINE_LENGTH_US * XTAL_MHZ)/2+0.5-10.0/8.0))

#define TOTAL_LINES 312
#define PROTOLINES 4
#define PROTOLINE_LENGTH_WORDS ((uint16_t)(LINE_LENGTH_US * XTAL_MHZ + 0.5))
#define PROTOLINE_WORD_ADDRESS(n) (PROTOLINE_LENGTH_WORDS * n)
#define PROTO_AREA_WORDS (PROTOLINE_LENGTH_WORDS * PROTOLINES)
#define INDEX_START_LONGWORDS ((PROTO_AREA_WORDS + 1) / 2)
#define INDEX_START_WORDS (INDEX_START_LONGWORDS * 2)
#define INDEX_START_BYTES (INDEX_START_WORDS * 2)

/// Picture area memory start point
#define PICLINE_START (INDEX_START_BYTES + TOTAL_LINES * 3 + 1)

/// Sync is always 0
#define SYNC_LEVEL  0x0000

/// Max blank level possible when burst is the smallest in relation to blank
#define BLANK_LEVEL 0x006b
#define BURST_LEVEL 0xe26b
// 3d -> (3;-3)
// 9 -> -1 (v), 7 -> 7 (u)
// burst 976b: couleurs palles, blanc ok
// burst d36b: (3;-3) ok, dd(-3;-3) -> lots of reds
// burst e26b: (-2; 2) couleurs patates, blanc ok
// #define BURST_LEVEL 0xe25b // value given in the vs23guide
// #define BLACK_LEVEL 0x006b
// #define WHITE_LEVEL 0x00de // 143 IRE should be 0x12e
// e2 -> 0111 0010 = +7 +2

/// Pattern generator microcode
/// ---------------------------
/// Bits 7:6  a=00|b=01|y=10|-=11
/// Bits 5:3  n pick bits 1..8
/// bits 2:0  shift 0..6
#define PICK_A (0<<6)
#define PICK_B (1<<6)
#define PICK_Y (2<<6)
#define PICK_NOTHING (3<<6)
#define PICK_BITS(a)(((a)-1)<<3)
#define SHIFT_BITS(a)(a)

void vs23_init_spi(spi_host_device_t host_id, const spi_bus_config_t *bus_config, spi_dma_chan_t dma_chan, int spics_io_num, int clock_speed_hz);

void vs23_enter_sram_mode(); //, uint8_t read_command, uint8_t write_command ?
void vs23_enter_fast_write_mode();

struct uv_tables_t {
    uint8_t v[4];
    int8_t u[4];
};

struct program_t {
    uint8_t op_1;
    uint8_t op_2;
    uint8_t op_3;
    uint8_t op_4;
};

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t pllclks_per_pixel;
    uint8_t bits_per_pixel;
    uint8_t ops_register;
    uint16_t flags;
    struct program_t program;
    struct uv_tables_t uv_tables;
} video_config_t;

void vs23_progressive_pal(video_config_t *video_config);

void set_pix_yuv(uint16_t x, uint16_t y, uint8_t yuv);
uint8_t rgb_to_yuv(uint8_t r, uint8_t g, uint8_t b);

#ifdef __cplusplus
}
#endif
