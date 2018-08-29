#include "spi_if.h"

#include "esp_log.h"

#include "driver/spi_common.h"
#include "driver/spi_master.h"

static const char *TAG = "spi_if";

spi_if_handle_t spi_if_init(int fre, int mode, int sck, int miso, int mosi, int cs)
{
    esp_err_t ret;
    spi_if_handle_t spi = NULL;
    spi_bus_config_t buscfg={
        .miso_io_num=miso,
        .mosi_io_num=mosi,
        .sclk_io_num=sck,
        .quadwp_io_num=-1,
        .quadhd_io_num=-1,
        .max_transfer_sz=1024
    };
    spi_device_interface_config_t devcfg={
        .clock_speed_hz=fre,           
        .mode=mode,                                   //SPI mode 0
        .spics_io_num=cs,               //CS pin
         .queue_size=1,                          //We want to be able to queue 7 transactions at a time
        // .pre_cb=lcd_spi_pre_transfer_callback,  //Specify pre-transfer callback to handle D/C line
    };
    //Initialize the SPI bus
    ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
    ESP_ERROR_CHECK(ret);
    //Attach the LCD to the SPI bus
    ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    return spi;
}

int spi_if_write(spi_if_handle_t spi, uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t;
    if (len==0) return 0;             //no need to send anything
    memset(&t, 0, sizeof(t));       //Zero out the transaction
    t.length=len*8;                 //Len is in bytes, transaction length is in bits.
    t.tx_buffer=data;               //Data
    // t.user=(void*)1;                //D/C needs to be set to 1
    ret=spi_device_transmit(spi, &t);  //Transmit!
    assert(ret==ESP_OK);            //Should have had no issues.

    return len;
}