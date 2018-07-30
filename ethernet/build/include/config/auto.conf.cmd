deps_config := \
	/home/xiongyu/esp/esp-idf/components/app_trace/Kconfig \
	/home/xiongyu/esp/esp-idf/components/aws_iot/Kconfig \
	/home/xiongyu/esp/esp-idf/components/bt/Kconfig \
	/home/xiongyu/esp/esp-idf/components/driver/Kconfig \
	/home/xiongyu/esp/esp-idf/components/esp32/Kconfig \
	/home/xiongyu/esp/esp-idf/components/esp_adc_cal/Kconfig \
	/home/xiongyu/esp/esp-idf/components/esp_http_client/Kconfig \
	/home/xiongyu/esp/esp-idf/components/ethernet/Kconfig \
	/home/xiongyu/esp/esp-idf/components/fatfs/Kconfig \
	/home/xiongyu/esp/esp-idf/components/freertos/Kconfig \
	/home/xiongyu/esp/esp-idf/components/heap/Kconfig \
	/home/xiongyu/esp/esp-idf/components/http_server/Kconfig \
	/home/xiongyu/esp/esp-idf/components/libsodium/Kconfig \
	/home/xiongyu/esp/esp-idf/components/log/Kconfig \
	/home/xiongyu/esp/esp-idf/components/lwip/Kconfig \
	/home/xiongyu/esp/esp-idf/components/mbedtls/Kconfig \
	/home/xiongyu/esp/esp-idf/components/mdns/Kconfig \
	/home/xiongyu/esp/esp-idf/components/openssl/Kconfig \
	/home/xiongyu/esp/esp-idf/components/pthread/Kconfig \
	/home/xiongyu/esp/esp-idf/components/spi_flash/Kconfig \
	/home/xiongyu/esp/esp-idf/components/spiffs/Kconfig \
	/home/xiongyu/esp/esp-idf/components/tcpip_adapter/Kconfig \
	/home/xiongyu/esp/esp-idf/components/vfs/Kconfig \
	/home/xiongyu/esp/esp-idf/components/wear_levelling/Kconfig \
	/home/xiongyu/esp/esp-idf/Kconfig.compiler \
	/home/xiongyu/ESP32/NetGate/ethernet/components/ETH/Kconfig.projbuild \
	/home/xiongyu/esp/esp-idf/components/bootloader/Kconfig.projbuild \
	/home/xiongyu/esp/esp-idf/components/esptool_py/Kconfig.projbuild \
	/home/xiongyu/ESP32/NetGate/ethernet/main/Kconfig.projbuild \
	/home/xiongyu/esp/esp-idf/components/partition_table/Kconfig.projbuild \
	/home/xiongyu/esp/esp-idf/Kconfig

include/config/auto.conf: \
	$(deps_config)


$(deps_config): ;
