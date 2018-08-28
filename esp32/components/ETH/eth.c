#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_err.h"
#include "esp_event_loop.h"
#include "esp_event.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_eth.h"

#include "rom/ets_sys.h"
#include "rom/gpio.h"

#include "soc/dport_reg.h"
#include "soc/io_mux_reg.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/gpio_reg.h"
#include "soc/gpio_sig_map.h"

#include "tcpip_adapter.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "soc/emac_ex_reg.h"
#include "driver/periph_ctrl.h"

#include "eth_phy/phy_lan8720.h"

#include "eth.h"

#include "lwip/dns.h"

#define DEFAULT_ETHERNET_PHY_CONFIG phy_lan8720_default_ethernet_config


static const char *TAG = "eth";

bool eth_static_ip;

#define PIN_PHY_POWER CONFIG_PHY_POWER_PIN
#define PIN_SMI_MDC   CONFIG_PHY_SMI_MDC_PIN
#define PIN_SMI_MDIO  CONFIG_PHY_SMI_MDIO_PIN

#ifdef CONFIG_PHY_USE_POWER_PIN
/* This replaces the default PHY power on/off function with one that
   also uses a GPIO for power on/off.

   If this GPIO is not connected on your device (and PHY is always powered), you can use the default PHY-specific power
   on/off function rather than overriding with this one.
*/
static void phy_device_power_enable_via_gpio(bool enable)
{
    assert(DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable);

    if (!enable) {
        /* Do the PHY-specific power_enable(false) function before powering down */
        DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable(false);
    }

    gpio_pad_select_gpio(PIN_PHY_POWER);
    gpio_set_direction(PIN_PHY_POWER,GPIO_MODE_OUTPUT);
    if(enable == true) {
        gpio_set_level(PIN_PHY_POWER, 1);
        ESP_LOGD(TAG, "phy_device_power_enable(TRUE)");
    } else {
        gpio_set_level(PIN_PHY_POWER, 0);
        ESP_LOGD(TAG, "power_enable(FALSE)");
    }

    // Allow the power up/down to take effect, min 300us
    vTaskDelay(1);

    if (enable) {
        /* Run the PHY-specific power on operations now the PHY has power */
        DEFAULT_ETHERNET_PHY_CONFIG.phy_power_enable(true);
    }
}
#endif

static void eth_gpio_config_rmii(void)
{
    // RMII data pins are fixed:
    // TXD0 = GPIO19
    // TXD1 = GPIO22
    // TX_EN = GPIO21
    // RXD0 = GPIO25
    // RXD1 = GPIO26
    // CLK == GPIO0
    phy_rmii_configure_data_interface_pins();
    // MDC is GPIO 23, MDIO is GPIO 18
    phy_rmii_smi_configure_pins(PIN_SMI_MDC, PIN_SMI_MDIO);
}

esp_err_t eth_install(system_event_cb_t cb, void *ctx)
{
    vTaskDelay(100 / portTICK_RATE_MS);
    esp_event_loop_init(cb, ctx);

    eth_config_t config = DEFAULT_ETHERNET_PHY_CONFIG;
    /* Set the PHY address in the example configuration */
    config.phy_addr = CONFIG_PHY_ADDRESS;
    config.gpio_config = eth_gpio_config_rmii;
    config.tcpip_input = tcpip_adapter_eth_input;
    config.clock_mode = ETH_CLOCK_GPIO0_IN;

#ifdef CONFIG_PHY_USE_POWER_PIN
    /* Replace the default 'power enable' function with an example-specific
       one that toggles a power GPIO. */
    config.phy_power_enable = phy_device_power_enable_via_gpio;
#endif

    if (esp_eth_init(&config) == ESP_OK) {
        esp_eth_enable();
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t eth_config(const char * hostname, uint32_t local_ip, uint32_t gateway, uint32_t subnet, uint32_t dns1, uint32_t dns2)
{
    esp_err_t err = ESP_OK;
    tcpip_adapter_ip_info_t info;
	
    if (hostname != NULL) {
        err = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, hostname);
    } else {
        err = tcpip_adapter_set_hostname(TCPIP_ADAPTER_IF_ETH, "NetGate");
    }
    
    if(err != ESP_OK){
        ESP_LOGE(TAG,"hostname could not be seted! Error: %d", err);
        return err;
    }   
    if(local_ip != (uint32_t)0x00000000){
        info.ip.addr = local_ip;
        info.gw.addr = gateway;
        info.netmask.addr = subnet;
    } else {
        info.ip.addr = 0;
        info.gw.addr = 0;
        info.netmask.addr = 0;
	}

    err = tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_ETH);
    if(err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED){
        ESP_LOGE(TAG,"DHCP could not be stopped! Error: %d", err);
        return err;
    }

    err = tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_ETH, &info);
    if(err != ESP_OK){
        ESP_LOGE(TAG,"STA IP could not be configured! Error: %d", err);
        return err;
}
    if(info.ip.addr){
        eth_static_ip = true;
    } else {
        err = tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_ETH);
        if(err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STARTED){
            ESP_LOGW(TAG,"DHCP could not be started! Error: %d", err);
            return err;
        }
        eth_static_ip = false;
    }

    ip_addr_t d;
    d.type = IPADDR_TYPE_V4;

    if(dns1 != (uint32_t)0x00000000) {
        // Set DNS1-Server
        d.u_addr.ip4.addr = dns1;
        dns_setserver(0, &d);
    }

    if(dns2 != (uint32_t)0x00000000) {
        // Set DNS2-Server
        d.u_addr.ip4.addr = dns2;
        dns_setserver(1, &d);
    }

    return ESP_OK;
}

esp_err_t eth_free(void)
{
    return esp_eth_disable();
}
