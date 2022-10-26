#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"

#include "esp_log.h"

#include "vs23_spi.h"
#include "vs23_driver.h"

spi_host_device_t _host_id;
int _clock_speed_hz;
int _spics_io_num;

void vs23_init_spi(
      spi_host_device_t host_id,
      const spi_bus_config_t *bus_config,
      spi_dma_chan_t dma_chan,
      int spics_io_num,
      int clock_speed_hz
) {
    _host_id = host_id;
    _spics_io_num = spics_io_num;
    _clock_speed_hz = clock_speed_hz;

    ESP_ERROR_CHECK(spi_bus_initialize(_host_id, bus_config, dma_chan));

    add_spi_device(_host_id, _clock_speed_hz, _spics_io_num);

    for (uint32_t i = 0; i < 0xefff; i++) {
        write_long(i, 0);
        if(i % 0xfff == 0) {
            vTaskDelay(1);
        }
    }

    write_status_register(VS23_STATUS_SPI_MODE_SEQUENTIAL);
}

void vs23_enter_sram_mode() {
    remove_spi_device();
    add_spi_device(_host_id, _clock_speed_hz, _spics_io_num);
    write_status_register(VS23_STATUS_SPI_MODE_SEQUENTIAL);
}

void vs23_enter_fast_write_mode() {
    remove_spi_device();
    add_spi_device(_host_id, 4 * _clock_speed_hz, _spics_io_num);
}

void _set_line_index(uint16_t line, uint16_t wordAddress) {
    uint32_t indexAddr = INDEX_START_BYTES + line * 3;
    write_byte(indexAddr++, 0); // Byteaddress and bits to 0, proto to 0
    write_byte(indexAddr++, wordAddress); // Actually it's wordaddress
    write_byte(indexAddr  , wordAddress >> 8);
}

void _set_pic_index(uint16_t line, uint32_t byteAddress) {
    uint16_t protoAddress = 0;
    uint32_t indexAddr = INDEX_START_BYTES + line * 3;
    write_byte(indexAddr++, (uint16_t)((byteAddress << 7) & 0x80) | (protoAddress & 0xf)); // Byteaddress LSB, bits to 0, proto to given value
    write_byte(indexAddr++, (uint16_t)(byteAddress >> 1)); //This is wordaddress
    write_byte(indexAddr,   (uint16_t)(byteAddress >> 9));
}

static uint32_t picline_length_bytes;
static uint32_t picture_start;

void vs23_progressive_pal(video_config_t *video_config) {
    write_ops_register(video_config->ops_register);

    uint16_t picture_length = video_config->pllclks_per_pixel * video_config->width / 8;
    // The first pixel of the picture area, the X direction.
    uint16_t start_picture = BLANK_END + (LINE_LENGTH - BLANK_END - picture_length) / 2;
    // The last pixel of the picture area.
    uint16_t end_picture = start_picture + picture_length;

    write_picture_start(start_picture - 1);
    write_picture_end(end_picture - 1);

    write_u_table(
        video_config->uv_tables.u[0],
        video_config->uv_tables.u[1],
        video_config->uv_tables.u[2],
        video_config->uv_tables.u[3]
    );
    write_v_table(
        video_config->uv_tables.v[0],
        video_config->uv_tables.v[1],
        video_config->uv_tables.v[2],
        video_config->uv_tables.v[3]
    );
    write_video_control1(video_config->flags, 0);

    write_line_length(LINE_LENGTH_PLL);
    write_program(
        video_config->program.op_1,
        video_config->program.op_2,
        video_config->program.op_3,
        video_config->program.op_4
    );

    write_picture_index_start_address(INDEX_START_LONGWORDS);

    // Enable the PAL Y lowpass filter
    // loss in sharpness, less aberrations
    write_block_move_control1(0, 0, 1<<4);
    write_video_control2(VS23_VIDEO_CONTROL2_VIDEO_ENABLED | VS23_VIDEO_CONTROL2_PAL_MODE, video_config->pllclks_per_pixel - 1, TOTAL_LINES - 1);

    // Construct protoline 0, used for picture lines
    uint16_t w = PROTOLINE_WORD_ADDRESS(0);
    for (uint16_t i = 0; i <= LINE_LENGTH; i++) write_word(w++, BLANK_LEVEL);
    // Set HSYNC
    w = PROTOLINE_WORD_ADDRESS(0);
    for (uint16_t i = 0; i < SYNC; i++) write_word(w++, SYNC_LEVEL);
    // Set color burst
    w = PROTOLINE_WORD_ADDRESS(0) + BURST;
    for (uint16_t i = 0; i < BURST_DURATION; i++) write_word(w++, BURST_LEVEL);

#ifdef DEBUG_COLORS
    // Background color white
    w = PROTOLINE_WORD_ADDRESS(0) + BLANK_END;
    for (uint16_t i = 0; i < (LINE_LENGTH - BLANK_END); i++) write_word(w++, 0x00ff);
    // U values with Y=.5
    w = PROTOLINE_WORD_ADDRESS(0) + BLANK_END + 20;
    for (int8_t u = -8; u < 8; u++) {
        for (uint8_t i = 0; i < 5; i++) write_word(w++, 0x007f | (0x0f00 & (u << 8)));
        w++;
    }
    w += 4;
    // V values with Y=.5
    for (int8_t v = -8; v < 8; v++) {
        for (uint8_t i = 0; i < 5; i++) write_word(w++, 0x007f | (0xf000 & (v << 12)));
        w++;
    }
#endif

    // Now let's construct protoline 1, this will become our short+short VSYNC line
    w = PROTOLINE_WORD_ADDRESS(1);
    for (uint16_t i = 0; i <= LINE_LENGTH; i++) write_word(w++, BLANK_LEVEL);
    w = PROTOLINE_WORD_ADDRESS(1);
    for (uint16_t i = 0; i < SHORT_SYNC; i++) write_word(w++, SYNC_LEVEL); // Short sync at the beginning of line
    w = PROTOLINE_WORD_ADDRESS(1) + LINE_HALF_LENGTH;
    for (uint16_t i = 0; i < SHORT_SYNC_M; i++) write_word(w++, SYNC_LEVEL); // Short sync at the middle of line

    // Now let's construct protoline 2, this will become our long+long VSYNC line
    w = PROTOLINE_WORD_ADDRESS(2);
    for (uint16_t i = 0; i <= LINE_LENGTH; i++) write_word(w++, BLANK_LEVEL);
    w = PROTOLINE_WORD_ADDRESS(2);
    for (uint16_t i = 0; i < LONG_SYNC; i++) write_word(w++, SYNC_LEVEL); // Long sync at the beginning of line
    w = PROTOLINE_WORD_ADDRESS(2) + LINE_HALF_LENGTH;
    for (uint16_t i = 0; i < LONG_SYNC_M; i++) write_word(w++, SYNC_LEVEL); // Long sync at the middle of line

    // Now let's construct protoline 3, this will become our long+short VSYNC line
    w = PROTOLINE_WORD_ADDRESS(3);
    for (uint16_t i = 0; i <= LINE_LENGTH; i++) write_word(w++, BLANK_LEVEL);
    w = PROTOLINE_WORD_ADDRESS(3);
    for (uint16_t i = 0; i < LONG_SYNC; i++) write_word(w++, SYNC_LEVEL); // Short sync at the beginning of line
    w = PROTOLINE_WORD_ADDRESS(3) + LINE_HALF_LENGTH;
    for (uint16_t i = 0; i < SHORT_SYNC_M; i++) write_word(w++, SYNC_LEVEL); // Long sync at the middle of line

    // Now set first eight lines of frame to point to PAL sync lines
    // Here the frame starts, lines 1 and 2
    _set_line_index(0, PROTOLINE_WORD_ADDRESS(2));
    _set_line_index(1, PROTOLINE_WORD_ADDRESS(2));
    _set_line_index(2, PROTOLINE_WORD_ADDRESS(3));
    _set_line_index(3, PROTOLINE_WORD_ADDRESS(1));
    _set_line_index(4, PROTOLINE_WORD_ADDRESS(1));
    for (uint16_t i = 5; i < 309; i++) {
        _set_line_index(i, PROTOLINE_WORD_ADDRESS(0));
    }
    _set_line_index(309, PROTOLINE_WORD_ADDRESS(1));
    _set_line_index(310, PROTOLINE_WORD_ADDRESS(1));
    _set_line_index(311, PROTOLINE_WORD_ADDRESS(1));

    // Set pic line indexes to point to protoline 0 and their individual picture line.
    uint16_t start_line = 22 + (288 - video_config->height) / 2;
    uint16_t end_line = start_line + video_config->height;
    picline_length_bytes = video_config->width * video_config->bits_per_pixel / 8 + 0.5;// + 1;
    uint16_t BEXTRA = 0;
    picture_start = PICLINE_START + (picline_length_bytes + BEXTRA) * start_line;

    for (uint16_t i = start_line; i < end_line; i++) {
        uint32_t picline_byte_address = PICLINE_START + (picline_length_bytes + BEXTRA) * i;
        _set_pic_index(i, picline_byte_address);
    }
}

void set_pix_yuv(uint16_t x, uint16_t y, uint8_t yuv) {
    uint32_t picline_byte_address = picture_start + picline_length_bytes * y;
    write_byte(picline_byte_address + x, yuv);
}

uint8_t rgb_to_yuv(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t _y = (uint8_t) ((76 * r + 150 * g + 19 * b) >> 8);
    ESP_LOGI("DRIVER", "y:\t%d\t%02x", _y, _y);
    int8_t _u = (int8_t) (((r << 7) - 107 * g - 20 * b) >> 8);
    ESP_LOGI("DRIVER", "u:\t%d\t%02x", _u, _u);
    int8_t _v = (int8_t) ((-43 * r - 84 * g + (b << 7)) >> 8);
    ESP_LOGI("DRIVER", "v:\t%d\t%02x", _v, _v);
    return ((_y >> 4) & 0x0f) | (_u & 0xb0) | ((_v >> 2) & 0x30);
}
