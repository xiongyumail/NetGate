#ifndef __SPI_INTERFACE_H__
#define __SPI_INTERFACE_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "driver/spi_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define spi_if_handle_t spi_device_handle_t
spi_if_handle_t spi_if_init(int fre, int mode, int sck, int miso, int mosi, int cs);

esp_err_t spi_if_write(spi_if_handle_t spi, uint8_t *data, int len);

#ifdef __cplusplus
}
#endif

#endif