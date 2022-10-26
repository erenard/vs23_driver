#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"

#include "vs23_spi.h"

spi_device_handle_t spi;

void add_spi_device(spi_host_device_t host_id, int clock_speed_hz, int spics_io_num) {
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = clock_speed_hz,
        .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_NO_DUMMY,
        .mode = 0,
        .spics_io_num = spics_io_num,
        .queue_size = 1,
        .address_bits = 24,
        .command_bits = 8,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(host_id, &devcfg, &spi));
}

void remove_spi_device() {
    spi_bus_remove_device(spi);
}

/***************/
/* SRAM Writes */
/***************/
void write_buffer(uint32_t address, void *tx_buffer, size_t length) {
  // SRAM Write are done in dual mode (address + data)
  // since quad modes fails on some patterns
  spi_transaction_t transaction = {
      .flags = SPI_TRANS_MODE_DIO | SPI_TRANS_MULTILINE_ADDR,
      .cmd = 0X22,
      .addr = address & 0x7ffff,
      .length = length,
      .tx_buffer = tx_buffer,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, &transaction));
}

void write_long(uint32_t address, uint32_t data) {
	uint32_t swapped = SPI_SWAP_DATA_TX(data, 32);
	write_buffer(address * 4, &swapped, 32);
}

void write_word(uint32_t address, uint16_t data) {
	uint16_t swapped = SPI_SWAP_DATA_TX(data, 16);
	write_buffer(address * 2, &swapped, 16);
}

void write_byte(uint32_t address, uint8_t data) {
	write_buffer(address, &data, 8);
}

/**************/
/* SRAM Reads */
/**************/
void read_buffer(uint32_t address, void *rx_buffer, size_t length) {
  // SRAM Read are done in:
  // - single mode for the address
  // - quad mode for the data
  spi_transaction_t transaction = {
      .flags = SPI_TRANS_MODE_QIO,
      .cmd = 0X6B,
      .addr = address & 0x7ffff,
      .rxlength = length,
      .rx_buffer = rx_buffer,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, &transaction));
}

uint8_t read_byte(uint32_t address) {
  uint8_t data;
  read_buffer(address, &data, 8);
  return data;
}

uint16_t read_word(uint32_t address) {
  uint16_t data;
  read_buffer(address * 2, &data, 16);
  return SPI_SWAP_DATA_RX(data, 16);
}

uint32_t read_long(uint32_t address) {
  uint32_t data;
  read_buffer(address * 4, &data, 32);
  return SPI_SWAP_DATA_RX(data, 32);
}

/***********/
/* PRIVATE */
/***********/
uint8_t _read_8bit_register(uint8_t command) {
  spi_transaction_ext_t transaction = {
      .base =
          {
              .flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_VARIABLE_ADDR,
              .cmd = command,
              .rxlength = 8,
          },
      .address_bits = 0,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&transaction));
  return transaction.base.rx_data[0];
}

uint16_t _read_16bit_register(uint8_t command) {
  uint16_t rx_data;
  spi_transaction_ext_t transaction = {
      .base =
          {
              .flags = SPI_TRANS_VARIABLE_ADDR,
              .cmd = command,
              .rxlength = 16,
              .rx_buffer = &rx_data,
          },
      .address_bits = 0,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&transaction));
  return SPI_SWAP_DATA_RX(rx_data, 16);
}

void _write_8bit_register(uint8_t command, uint8_t value) {
  spi_transaction_ext_t transaction = {
      .base =
          {
              .flags = SPI_TRANS_VARIABLE_ADDR,
              .cmd = command,
              .length = 8,
              .tx_buffer = &value,
          },
      .address_bits = 0,
  };
  ESP_LOGI("VS23_REG", "0x%02x <- 0x%02x", command, value);
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&transaction));
}

void _write_16bit_register(uint8_t command, uint16_t value) {
  uint16_t tx_buffer = SPI_SWAP_DATA_TX(value, 16);
  spi_transaction_ext_t transaction = {
      .base =
          {
              .flags = SPI_TRANS_VARIABLE_ADDR,
              .cmd = command,
              .length = 16,
              .tx_buffer = &tx_buffer,
          },
      .address_bits = 0,
  };
  ESP_LOGI("VS23_REG", "0x%02x <- 0x%04x", command, value);
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&transaction));
}

void _write_32bit_register(uint8_t command, uint32_t value) {
  uint32_t tx_buffer = SPI_SWAP_DATA_TX(value, 32);
  spi_transaction_ext_t transaction = {
      .base =
          {
              .flags = SPI_TRANS_VARIABLE_ADDR,
              .cmd = command,
              .length = 32,
              .tx_buffer = &tx_buffer,
          },
      .address_bits = 0,
  };
  ESP_LOGI("VS23_REG", "0x%02x <- 0x%08x", command, value);
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&transaction));
}

void _write_40bit_register(uint8_t command, uint16_t source, uint16_t target,  uint8_t value) {
  uint8_t tx_buffer[] = {source >> 8, source & 0xff, target >> 8, target & 0xff, value};
  spi_transaction_ext_t transaction = {
      .base =
          {
              .flags = SPI_TRANS_VARIABLE_ADDR,
              .cmd = command,
              .length = 40,
              .tx_buffer = &tx_buffer,
          },
      .address_bits = 0,
  };
  ESP_LOGI("VS23_REG", "0x%02x <- 0x%04x%04x%02x", command, source, target, value);
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&transaction));
}

/*************/
/* Registers */
/*************/
uint16_t read_device_id() { return _read_16bit_register(0X9F); }

void write_status_register(uint8_t status) {
  _write_8bit_register(0X01, status);
}

uint8_t read_status_register() { return _read_8bit_register(0X05); }

void write_ops_register(uint8_t status) {
  _write_8bit_register(0XB8, 0X0F & status);
}

uint8_t read_ops_register() { return _read_8bit_register(0XB7); }

void write_picture_start(uint16_t start) {
  _write_16bit_register(0X28, 0X0FFF & start);
}

void write_picture_end(uint16_t end) {
  _write_16bit_register(0X29, 0X0FFF & end);
}

void write_line_length(uint16_t line_length) {
  _write_16bit_register(0X2A, 0X1FFF & line_length);
}

void write_video_control1(uint16_t flags, uint16_t dac_divider) {
  uint16_t control1 = flags | (dac_divider & 0X0ff8);
  _write_16bit_register(0X2B, control1);
}

void write_picture_index_start_address(uint16_t start_address) {
  _write_16bit_register(0X2C, 0X3FFF & start_address);
}

void write_video_control2(uint16_t flags, uint8_t program_length,
                          uint16_t line_count) {
  uint16_t control2 = flags | ((((uint16_t)program_length) & 0X000f) << 10) |
                      (line_count & 0X03ff);
  _write_16bit_register(0X2D, control2);
}

void write_v_table(uint8_t zero, uint8_t one, uint8_t two, uint8_t three) {
  uint16_t table = ((three << 12) & 0xf000) | ((two << 8) & 0xf00) | ((one << 4) & 0xf0) | (zero & 0xf);
  _write_16bit_register(0X2F, table);
}

void write_u_table(int8_t zero, int8_t one, int8_t two, int8_t three) {
  uint16_t table = ((three << 12) & 0xf000) | ((two << 8) & 0xf00) | ((one << 4) & 0xf0) | (zero & 0xf);
  _write_16bit_register(0X2E, table);
}

uint8_t make_cycle(uint8_t pick, uint8_t amount, uint8_t shift) {
  return pick | ((amount & 0X07) << 3) | (shift & 0X07);
}

void write_program(uint8_t cycle_1, uint8_t cycle_2, uint8_t cycle_3,
                   uint8_t cycle_4) {
  uint32_t program = cycle_4 << 24 | cycle_3 << 16 | cycle_2 << 8 | cycle_1;
  _write_32bit_register(0X30, program);
}

uint16_t read_current_line_pll_lock() { return _read_16bit_register(0X53); }

void write_block_move_control1(uint16_t source, uint16_t target, uint8_t flags) {
  _write_40bit_register(0X34, source, target, flags);
}

#ifdef SPI_TESTS
// OK 32MHz, used to check other methods
uint32_t read_safe(uint32_t address) {
  uint32_t rx_buffer;
  spi_transaction_t transaction = {
      .flags = 0,
      .cmd = 0X03,
      .addr = address,
      .length = 32,
      .rxlength = 32,
      .rx_buffer = &rx_buffer,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, &transaction));
  return rx_buffer;
};

// OK 32MHz, used to check other methods
void write_safe(uint32_t address, uint32_t data) {
  spi_transaction_t transaction = {
      .flags = 0,
      .cmd = 0X02,
      .addr = address,
      .length = 32,
      .tx_buffer = &data,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, &transaction));
};

// OK 16MHz, KO 24MHz
uint32_t read_test_ddio(uint32_t address) {
  uint32_t rx_buffer;
  spi_transaction_ext_t transaction = {
      .dummy_bits = 8,
      .base =
          {
              .flags = SPI_TRANS_MODE_DIO | SPI_TRANS_MULTILINE_ADDR |
                       SPI_TRANS_VARIABLE_DUMMY,
              .cmd = 0XBB,
              .addr = address - 1,
              .length = 32,
              .rxlength = 32,
              .rx_buffer = &rx_buffer,
          },
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&transaction));
  return rx_buffer;
};

// OK 32MHz
void write_test_ddio(uint32_t address, uint32_t data) {
  spi_transaction_t transaction = {
      .flags = SPI_TRANS_MODE_DIO | SPI_TRANS_MULTILINE_ADDR,
      .cmd = 0X22,
      .addr = address,
      .length = 32,
      .tx_buffer = &data,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, &transaction));
};

// OK 32MHz
uint32_t read_test_sqio(uint32_t address) {
  uint32_t rx_buffer;
  spi_transaction_t transaction = {
      .flags = SPI_TRANS_MODE_QIO,
      .cmd = 0X6B,
      .addr = address,
      .length = 32,
      .rxlength = 32,
      .rx_buffer = &rx_buffer,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, &transaction));
  return rx_buffer;
};

// OK 16MHz, KO 24MHz
void write_test_sqio(uint32_t address, uint32_t data) {
  spi_transaction_ext_t transaction = {
      .base =
          {
              .flags = SPI_TRANS_MODE_QIO,
              .cmd = 0X32,
              .addr = address,
              .length = 32,
              .tx_buffer = &data,
          },
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&transaction));
};

// KO
uint32_t read_test_qqio(uint32_t address) {
  uint32_t rx_buffer;
  spi_transaction_ext_t t;
  memset(&t, 0, sizeof(t));
  t.dummy_bits = 8;
  t.base.flags =
      SPI_TRANS_MODE_QIO | SPI_TRANS_MULTILINE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
  t.base.cmd = 0XEB;
  t.base.addr = address;
  t.base.rxlength = 32;
  t.base.rx_buffer = &rx_buffer;
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&t));
  return rx_buffer;
};

void read_test_sqio_buffer(uint32_t address, uint8_t *rx_buffer,
                           size_t rx_length) {
  spi_transaction_t transaction = {
      .flags = SPI_TRANS_MODE_QIO,
      .cmd = 0X6B,
      .addr = address,
      .rxlength = rx_length,
      .rx_buffer = rx_buffer,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, &transaction));
};
void read_test_qqio_buffer(uint32_t address, uint8_t *rx_buffer,
                           size_t rx_length) {
  spi_transaction_ext_t t;
  memset(&t, 0, sizeof(t));
  t.dummy_bits = 8;
  t.base.flags =
      SPI_TRANS_MODE_QIO | SPI_TRANS_MULTILINE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
  t.base.cmd = 0XEB;
  t.base.addr = address;
  t.base.rxlength = rx_length;
  t.base.rx_buffer = rx_buffer;
  ESP_ERROR_CHECK(spi_device_transmit(spi, (spi_transaction_t *)&t));
};

// OK 32 MHz
// KO fails some patterns
void write_test_qqio(uint32_t address, uint32_t data) {
  spi_transaction_t transaction = {
      .flags = SPI_TRANS_MODE_QIO | SPI_TRANS_MULTILINE_ADDR,
      .cmd = 0XB2,
      .addr = address,
      .length = 32,
      .tx_buffer = &data,
  };
  ESP_ERROR_CHECK(spi_device_transmit(spi, &transaction));
};
#endif
