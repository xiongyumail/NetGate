#include <string.h>
#include <sys/socket.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/ringbuf.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_spi_flash.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "eth.h"
#include "driver/uart.h"

#include "fifo.h"

#include "lwip/dns.h"

#include "spi_if.h"

#define LOGO \
"\
+--------------------------------------------------------------------------------------------------+\n\
|                                                                                                  |\n\
|                                                                                                  |\n\
|      XX      X    XXXXXXXX     XXXXXXXXXX     XXXXXXX         XX        XXXXXXXXXX   XXXXXXXXX   |\n\
|      XX     XX    X                X         X                X XX           X       X           |\n\
|      X X    X     X                X        X                X   XX          X       X           |\n\
|     XX XX   X     X                X       XX     XXXXXXX   XX    XX         X       X           |\n\
|     X   X   X     XXXXXXXX         X       X         XX     XXXXXXXX         X       XXXXXXXXX   |\n\
|     X    X XX     X                X       XX         X    XX      X         X       X           |\n\
|     X    XXX      X                X        XXX      XX    X       XX        X       X           |\n\
|    XX     XX      XXXXXXXX         X           X X XXX    XX        X        X       XXXXXXXXX   |\n\
|                                                                                                  |\n\
|                                                                                                  |\n\
+--------------------------------------------------------------------------------------------------+\n\
"


#ifdef CONFIG_ETHERNET
#define NET_INTERFACE  TCPIP_ADAPTER_IF_ETH
#elif  CONFIG_WIFI

#ifdef CONFIG_WIFI_STATION
#define NET_INTERFACE  TCPIP_ADAPTER_IF_STA
#elif  CONFIG_WIFI_AP
#define NET_INTERFACE  TCPIP_ADAPTER_IF_AP
#endif

#define WIFI_MODE_AP   CONFIG_WIFI_MODE_AP //TRUE:AP FALSE:STA
#define WIFI_SSID      CONFIG_WIFI_SSID
#define WIFI_PASS      CONFIG_WIFI_PASSWORD
#endif




#define SERVER_IP "192.168.0.101"
#define SERVER_PORT 8000
#define RECV_BUF_SIZE 1024

typedef struct {
  uint8_t *data;
  int len;
} buf_t;

fifo_handle_t tcp_fifo_handle;
fifo_handle_t uart_fifo_handle;

spi_if_handle_t spi_if_handle;

bool static_ip;

static const char *TAG = "main";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t net_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch (event -> event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(net_event_group, CONNECTED_BIT);
            ESP_LOGI(TAG, "got ip:%s",
                     ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            xEventGroupClearBits(net_event_group, CONNECTED_BIT);
            esp_wifi_connect();
            break;

        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                     MAC2STR(event->event_info.sta_connected.mac),
                     event->event_info.sta_connected.aid);
            break;

        case SYSTEM_EVENT_AP_STADISCONNECTED:
            xEventGroupClearBits(net_event_group, CONNECTED_BIT);
            ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                     MAC2STR(event->event_info.sta_disconnected.mac),
                     event->event_info.sta_disconnected.aid);
            break;

        case SYSTEM_EVENT_ETH_START:
            ESP_LOGI(TAG, "ETH Started");
            break;

        case SYSTEM_EVENT_ETH_CONNECTED:
            ESP_LOGI(TAG, "ETH Connected");
            break;

        case SYSTEM_EVENT_ETH_GOT_IP:
            xEventGroupSetBits(net_event_group, CONNECTED_BIT);
            ESP_LOGI(TAG, "ETH_GOT_IP");
            break;

        case SYSTEM_EVENT_ETH_DISCONNECTED:
            xEventGroupClearBits(net_event_group, CONNECTED_BIT);
            ESP_LOGI(TAG, "ETH Disconnected");
            break;

        case SYSTEM_EVENT_ETH_STOP:
            ESP_LOGI(TAG, "ETH Stopped");
            break;

        default:
            break;
    }

    return ESP_OK;
}
#ifdef  CONFIG_WIFI
static void wifi_init_sta()
{
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             WIFI_SSID, WIFI_PASS);
}
#endif
static esp_err_t net_config(tcpip_adapter_if_t interface, const char * hostname, uint32_t local_ip, uint32_t gateway, uint32_t subnet, uint32_t dns1, uint32_t dns2)
{
    esp_err_t err = ESP_OK;
    tcpip_adapter_ip_info_t info;
	
    if (hostname != NULL) {
        err = tcpip_adapter_set_hostname(interface, hostname);
    } else {
        err = tcpip_adapter_set_hostname(interface, "NetGate");
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

    err = tcpip_adapter_dhcpc_stop(interface);
    if(err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STOPPED){
        ESP_LOGE(TAG,"DHCP could not be stopped! Error: %d", err);
        return err;
    }

    err = tcpip_adapter_set_ip_info(interface, &info);
    if(err != ESP_OK){
        ESP_LOGE(TAG,"STA IP could not be configured! Error: %d", err);
        return err;
}
    if(info.ip.addr){
        static_ip = true;
    } else {
        err = tcpip_adapter_dhcpc_start(interface);
        if(err != ESP_OK && err != ESP_ERR_TCPIP_ADAPTER_DHCP_ALREADY_STARTED){
            ESP_LOGW(TAG,"DHCP could not be started! Error: %d", err);
            return err;
        }
        static_ip = false;
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

static void tcp_client_task(void *pvParameter)
{
    /* Wait for the callback to set the CONNECTED_BIT in the
         event group.
      */
    ESP_LOGI(TAG, "Wait for ESP32 Connect to NET!");
    xEventGroupWaitBits(net_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "ESP32 Connected to NET ! Start TCP Server....");

    while (1) {
        vTaskDelay(2000 / portTICK_RATE_MS);
        int sockfd = 0, iResult = 0;

        struct sockaddr_in serv_addr;
        fd_set read_set, write_set, error_set;

        sockfd = socket(AF_INET, SOCK_STREAM, 0);

        if (sockfd == -1) {
            ESP_LOGE(TAG, "create socket failed!");
            continue;
        }

        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(SERVER_PORT);
        serv_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

        int conn_ret = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

        if (conn_ret == -1) {
            ESP_LOGE(TAG, "connect to server failed! errno=%d", errno);
            close(sockfd);
            continue;
        } else {
            ESP_LOGI(TAG, "connected to tcp server OK, sockfd:%d...", sockfd);
        }

        // set keep-alive
#if 1
        int keepAlive = 1;
        int keepIdle = 10;
        int keepInterval = 1;
        int keepCount = 5;
        int ret = 0;
        ret = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive));
        ret |= setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(keepIdle));
        ret |= setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(keepInterval));
        ret |= setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(keepCount));

        if (ret) {
            ESP_LOGE(TAG, "set socket keep-alive option failed...");
            close(sockfd);
            continue;
        }

        ESP_LOGI(TAG, "set socket option ok...");
#endif

        struct timeval timeout;
        timeout.tv_sec = 3;
        timeout.tv_usec = 0;

        char *recv_buf = (char *)calloc(RECV_BUF_SIZE, 1);

        if (recv_buf == NULL) {
            ESP_LOGE(TAG, "alloc failed, reset chip...");
            esp_restart();
        }

        while (1) {
            FD_ZERO(&read_set);
            FD_SET(sockfd, &read_set);
            write_set = read_set;
            error_set = read_set;
            iResult = select(sockfd + 1, &read_set, &write_set, &error_set, &timeout);

            if (iResult == -1) {
                ESP_LOGE(TAG, "TCP client select failed");
                break; // reconnect
            }

            if (iResult == 0) {
                ESP_LOGI(TAG, "TCP client A select timeout occurred");
                continue;
            }

            if (FD_ISSET(sockfd, &error_set)) {
                // error happen
                ESP_LOGE(TAG, "select error_happend");
                break;
            }

            if (FD_ISSET(sockfd, &read_set)) {
                // readable
                int recv_ret = recv(sockfd, recv_buf, RECV_BUF_SIZE, 0);

                if (recv_ret > 0) {
                    //do more chores
                    if(-1 == fifo_send(tcp_fifo_handle, (uint8_t *)recv_buf, recv_ret, 20)) {
                        ESP_LOGE(TAG, "%d tcp fifo Send error", recv_ret); 
                    }
                } else {
                    ESP_LOGW(TAG, "close tcp transmit, would close socket...");
                    break;
                }
            }

            if (FD_ISSET(sockfd, &write_set)) {
                // writable
                fifo_buf_t *buf;
                // send client data to tcp server
                buf = fifo_recv(uart_fifo_handle, 20);
                if (NULL != buf) {
                    char *send_buf_offset = (char *)buf->data;
                    int len = buf->len;
                    while(len > 0) {
                        int send_ret = send(sockfd, send_buf_offset, len, 0);
                        if (send_ret == -1) {
                            ESP_LOGE(TAG, "send data to tcp server failed");
                            break;
                        } else {
                            send_buf_offset += send_ret;
                            len -= send_ret;
                        }
                    } 
                    fifo_buf_free(buf);
                    if (0 != len) {
                        break;
                    }             
                }
            }
        }

        if (sockfd > 0) {
            close(sockfd);
            ESP_LOGW(TAG, "close socket , sockfd:%d", sockfd);
        }
        free(recv_buf);
        recv_buf = NULL;
        ESP_LOGW(TAG, "reset tcp client and reconnect to tcp server...");
    } // end

    ESP_LOGI(TAG,"A stop nonblock...\n");
    vTaskDelete(NULL);
    return;
}

#define BUF_SIZE (1024)

static void uart_task()
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, BUF_SIZE * 2, 0, NULL, 0);

    // Configure a temporary buffer for the incoming data
    uint8_t *recv_data = (uint8_t *) malloc(BUF_SIZE);

    if (recv_data == NULL) {
        ESP_LOGE(TAG, "alloc failed, reset chip...");
        esp_restart();
    }

    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(UART_NUM_0, recv_data, BUF_SIZE, 20 / portTICK_RATE_MS);
        
        if (len) {
            if(-1 == fifo_send(uart_fifo_handle, recv_data, len, 20)) {
                ESP_LOGE(TAG, "%d uart fifo Send error", len); 
            }
            spi_if_write(spi_if_handle, recv_data, len);
        }
        // Write data to the UART
        fifo_buf_t *buf;
        buf = fifo_recv(tcp_fifo_handle, 20);
        if (NULL != buf) {
            uart_write_bytes(UART_NUM_0, (const char *)buf->data, buf->len);
            fifo_buf_free(buf);
        }
    }
}

void app_main()
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

	/* Print chip information */
	esp_chip_info_t chip_info;
    printf(LOGO);
	esp_chip_info(&chip_info);
	ESP_LOGI(TAG,"This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
			chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
					(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

	ESP_LOGI(TAG,"silicon revision %d, ", chip_info.revision);

	ESP_LOGI(TAG,"%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
			(chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    tcpip_adapter_init();
    net_event_group = xEventGroupCreate();

    if (TCPIP_ADAPTER_IF_ETH == NET_INTERFACE) {
        eth_install(event_handler, NULL);
    } else {
        #ifdef CONFIG_WIFI_STATION
        wifi_init_sta();
        #endif
    }
    
    net_config(NET_INTERFACE, "ESP32-GateWay", inet_addr("192.168.0.102"), inet_addr("192.168.0.1"), inet_addr("255.255.255.0"), inet_addr("192.168.0.1"), 0);

    tcp_fifo_handle = fifo_create(100);
    uart_fifo_handle = fifo_create(100);
    spi_if_handle = spi_if_init(10000000, 0, 14, 12, 13, 15);
    xTaskCreate(&uart_task, "uart_task", 2048, NULL, 5, NULL);
    xTaskCreate(&tcp_client_task, "tcp_client_task", 4096, NULL, 5, NULL);

    // new more task to do other chores
    while (1) {
        vTaskDelay(1000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "current heap size:%d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
  }