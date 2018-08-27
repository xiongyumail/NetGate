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

#include "nvs.h"
#include "nvs_flash.h"

#include "eth.h"
#include "driver/uart.h"

#define EXAMPLE_ESP_WIFI_MODE_AP   CONFIG_ESP_WIFI_MODE_AP //TRUE:AP FALSE:STA
#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD

#define SERVER_IP "192.168.0.101"
#define SERVER_PORT 8000
#define RECV_BUF_SIZE 1024

typedef struct {
  uint8_t *data;
  int len;
} buf_t;

RingbufHandle_t tcp_ring_buf;
RingbufHandle_t uart_ring_buf;

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
            ESP_LOGI(TAG, "got ip:%s",
                     ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            break;

        case SYSTEM_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "station:"MACSTR" join, AID=%d",
                     MAC2STR(event->event_info.sta_connected.mac),
                     event->event_info.sta_connected.aid);
            break;

        case SYSTEM_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "station:"MACSTR"leave, AID=%d",
                     MAC2STR(event->event_info.sta_disconnected.mac),
                     event->event_info.sta_disconnected.aid);
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
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

void wifi_init_sta()
{
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    ESP_LOGI(TAG, "connect to ap SSID:%s password:%s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
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

        char *send_buf = NULL;
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
                    BaseType_t res = xRingbufferSend(tcp_ring_buf, recv_buf, recv_ret, 20 / portTICK_RATE_MS);
                    if(res != pdTRUE) {
                        ESP_LOGE(TAG, "%d RingbufferSend error",recv_ret); 
                    }
                } else {
                    ESP_LOGW(TAG, "close tcp transmit, would close socket...");
                    break;
                }
            }

            if (FD_ISSET(sockfd, &write_set)) {
                // writable
                size_t len;
                // send client data to tcp server
                send_buf = (char *) xRingbufferReceive(uart_ring_buf, &len, 20 / portTICK_RATE_MS);
                if (NULL != send_buf) {
                    char *send_buf_offset = send_buf;
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
                    vRingbufferReturnItem(uart_ring_buf, send_buf); 
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

    printf("A stop nonblock...\n");
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
    uint8_t *send_data = NULL;
    size_t len;

    if (recv_data == NULL) {
        ESP_LOGE(TAG, "alloc failed, reset chip...");
        esp_restart();
    }

    while (1) {
        // Read data from the UART
        len = uart_read_bytes(UART_NUM_0, recv_data, BUF_SIZE, 20 / portTICK_RATE_MS);
        
        if (len) {
            BaseType_t res = xRingbufferSend(uart_ring_buf, recv_data, len, 20 / portTICK_RATE_MS);
            if(res != pdTRUE) {
                ESP_LOGE(TAG, "%d RingbufferSend error",len); 
            }
        }
        // Write data to the UART
        send_data = (uint8_t *) xRingbufferReceive(tcp_ring_buf, &len, 20 / portTICK_RATE_MS);
        if (NULL != send_data) {
          uart_write_bytes(UART_NUM_0, (const char *)send_data, len);
          vRingbufferReturnItem(tcp_ring_buf, send_data);
        }
    }
}

void app_main()
{
    tcpip_adapter_init();
    net_event_group = xEventGroupCreate();
    eth_install(event_handler, NULL);

    tcp_ring_buf = xRingbufferCreate(1024, RINGBUF_TYPE_BYTEBUF);
    uart_ring_buf = xRingbufferCreate(1024, RINGBUF_TYPE_BYTEBUF);
    xTaskCreate(&uart_task, "uart_task", 2048, NULL, 5, NULL);
    xTaskCreate(&tcp_client_task, "tcp_client_task", 4096, NULL, 5, NULL);

    // new more task to do other chores
    while (1) {
        vTaskDelay(3000 / portTICK_RATE_MS);
        ESP_LOGI(TAG, "current heap size:%d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
  }