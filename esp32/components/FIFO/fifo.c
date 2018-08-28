#include "fifo.h"

#include "esp_log.h"

static const char *TAG = "fifo";

fifo_handle_t fifo_create(uint32_t size)
{
    return (fifo_handle_t)xQueueCreate(size, sizeof(fifo_buf_t *));
}

int fifo_send(fifo_handle_t fifo_handle, uint8_t *data, int len, fifo_wait_t time)
{
    if (0 == len) {
        return 0;
    }
    if (NULL == fifo_handle || NULL == data) {
        ESP_LOGE(TAG, "point NULL");
        return -1;
    }

    fifo_buf_t *buf = (fifo_buf_t *)malloc(sizeof(fifo_buf_t));
    if (buf == NULL) {
        ESP_LOGE(TAG, "fifo_buf malloc failed");
        return -1;
    }
    buf->data = (uint8_t *)malloc(sizeof(uint8_t)*len);
    if (buf->data == NULL) {
        ESP_LOGE(TAG, "data space malloc failed");
        free(buf);
        return -1;
    }
    fifo_memcpy(buf->data, data, len);
    buf->len = len;
    if (pdPASS != xQueueSend((QueueHandle_t)fifo_handle, &buf, time / portTICK_RATE_MS)) {
        ESP_LOGE(TAG, "queue send failed");
        free(buf->data);
        free(buf);
        return -1;
    }

    return len;
}

fifo_buf_t *fifo_recv(fifo_handle_t fifo_handle, fifo_wait_t time)
{
    fifo_buf_t *buf;
    if (NULL == fifo_handle) {
        ESP_LOGE(TAG, "point NULL");
        return NULL;
    }

    if (pdPASS != xQueueReceive((QueueHandle_t)fifo_handle, &buf, time / portTICK_RATE_MS)) {
        return NULL;
    }

    return buf;
}

void fifo_buf_free(fifo_buf_t *buf)
{
    if (NULL == buf) {
        ESP_LOGE(TAG, "point NULL");
        return ;
    }
    free(buf->data);
    free(buf);
}

void fifo_delete(fifo_handle_t fifo_handle)
{
    vQueueDelete((QueueHandle_t)fifo_handle);
}

