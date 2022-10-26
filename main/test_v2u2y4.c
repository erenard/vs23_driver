#define SPI_MASTER_FREQ_1M 1000000

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "soc/spi_pins.h"
#include "freertos/task.h"

#include "vs23_driver.h"

void app_main(void) {
    gpio_reset_pin(SPI3_IOMUX_PIN_NUM_CS);
    gpio_reset_pin(SPI3_IOMUX_PIN_NUM_CLK);
    gpio_reset_pin(SPI3_IOMUX_PIN_NUM_MISO);
    gpio_reset_pin(SPI3_IOMUX_PIN_NUM_MOSI);
    gpio_reset_pin(SPI3_IOMUX_PIN_NUM_HD);
    gpio_reset_pin(SPI3_IOMUX_PIN_NUM_WP);

	  spi_bus_config_t buscfg = {
	      .flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_IOMUX_PINS |
	               SPICOMMON_BUSFLAG_QUAD,
	      .miso_io_num = SPI3_IOMUX_PIN_NUM_MISO,
	      .mosi_io_num = SPI3_IOMUX_PIN_NUM_MOSI,
	      .sclk_io_num = SPI3_IOMUX_PIN_NUM_CLK,
	      .quadwp_io_num = SPI3_IOMUX_PIN_NUM_WP,
	      .quadhd_io_num = SPI3_IOMUX_PIN_NUM_HD,
	  };
	  vs23_init_spi(
	  		SPI3_HOST,
	  		&buscfg,
	  		SPI_DMA_CH_AUTO,
	  		SPI3_IOMUX_PIN_NUM_CS,
	  		SPI_MASTER_FREQ_1M
	  );

	  video_config_t video_config = {
	  	.ops_register =
	  			VS23_IC1_DISABLED |
	  			VS23_IC2_DISABLED |
	  			VS23_IC3_DISABLED,
	  	.flags =
	  			VS23_VIDEO_CONTROL1_SELECT_PLL_CLOCK |
	  			VS23_VIDEO_CONTROL1_PLL_ENABLED,
	  	.width = 430,
	  	.height = 260,
	  	.pllclks_per_pixel = 4,
	  	.bits_per_pixel = 8,
	  	.program = {
		  		.op_1 = PICK_B + PICK_BITS(2) + SHIFT_BITS(2),
		  		.op_2 = PICK_A + PICK_BITS(2) + SHIFT_BITS(2),
		  		.op_3 = PICK_Y + PICK_BITS(4) + SHIFT_BITS(4),
		  		.op_4 = PICK_NOTHING,
	  	}
	  };
		vs23_progressive_pal(&video_config);

		uint16_t x_inc = 26, y_inc = 16, x_pos = 0, y_pos = 0;
		for (uint8_t y = 0; y < 8; y++) {
		for (int8_t u = -2; u < 2; u++) {
		for (int8_t v = -2; v < 2; v++) {
		uint8_t yuv = (v << 6 & 0xc0) | (u << 4 & 0x30) | (y & 0x0f);
		x_pos = ((y % 3) * 5 + 3 + u) * x_inc;
		y_pos = ((y / 3) * 5 + 2 - v) * y_inc;
		for (uint16_t x = 0; x < x_inc - 1; x++) {
		for (uint16_t y = 0; y < y_inc - 1; y++) {
		set_pix_yuv(x + x_pos, y + y_pos, yuv);
		}
		}
		}
		}
		vTaskDelay(1);
		}
		// grays
		for (uint8_t yuv = 0; yuv < 8; yuv++) {
		x_pos = ((yuv % 4) + 11) * x_inc;
		y_pos = ((yuv / 4) + 11) * y_inc;
		for (uint16_t x = 0; x < x_inc - 1; x++) {
		for (uint16_t y = 0; y < y_inc - 1; y++) {
		set_pix_yuv(x + x_pos, y + y_pos, yuv);
		}
		}
		}
}
