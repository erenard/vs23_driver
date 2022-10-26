#include <stdlib.h>

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// #define SPI_TESTS

#define VS23_STATUS_SPI_HOLD_DISABLED    (1<<0) // Hold functionality functionality in Single and Dual mode SPI operations.
#define VS23_STATUS_USER1                (1<<1) // User assignable bit.
#define VS23_STATUS_USER2                (1<<2) // User assignable bit.
#define VS23_STATUS_USER3                (1<<3) // User assignable bit.
#define VS23_STATUS_SPI_FAST_WRITE_VIDEO (1<<4) // Fast write, but needs to be 32bit aligned.
#define VS23_STATUS_RESERVED             (1<<5)
#define VS23_STATUS_SPI_MODE_SEQUENTIAL  (1<<6) // Auto increment, full memory access.
#define VS23_STATUS_SPI_MODE_PAGE        (1<<7) // Auto increment, page memory access.

void add_spi_device(spi_host_device_t host_id, int clock_speed_hz, int spics_io_num);
void remove_spi_device();

uint8_t read_status_register();
void write_status_register(uint8_t status);
uint16_t read_device_id();
uint8_t read_ops_register();
void write_ops_register(uint8_t status);
void write_picture_start(uint16_t start);
void write_picture_end(uint16_t end);
void write_line_length(uint16_t line_length);
void write_video_control1(uint16_t flags, uint16_t dac_divider);
void write_picture_index_start_address(uint16_t start_address);
void write_video_control2(uint16_t flags, uint8_t program_length, uint16_t line_count);
void write_v_table(uint8_t a, uint8_t b, uint8_t c, uint8_t d);
void write_u_table(int8_t a, int8_t b, int8_t c, int8_t d);
uint8_t make_cycle(uint8_t pick, uint8_t amount, uint8_t shift);
void write_program(uint8_t cycle_1, uint8_t cycle_2, uint8_t cycle_3, uint8_t cycle_4);
uint16_t read_current_line_pll_lock();
void write_block_move_control1(uint16_t source, uint16_t target, uint8_t flags);

void write_buffer(uint32_t address, void *data, size_t length);
void read_buffer(uint32_t address, void *data, size_t length);
void write_long(uint32_t address, uint32_t data);
uint32_t read_long(uint32_t address);
void write_word(uint32_t address, uint16_t data);
uint16_t read_word(uint32_t address);
void write_byte(uint32_t address, uint8_t data);
uint8_t read_byte(uint32_t address);

#ifdef __cplusplus
}
#endif
