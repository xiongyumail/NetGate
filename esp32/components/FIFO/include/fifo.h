#ifndef __FIFO_H__
#define __FIFO_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define fifo_memcpy memcpy

#define fifo_handle_t QueueHandle_t

#define fifo_wait_t uint32_t

typedef struct fifo_buf{
    uint8_t *data;
    int len;
} fifo_buf_t;

fifo_handle_t fifo_create(uint32_t size);

int fifo_send(fifo_handle_t fifo_handle, uint8_t *data, int len, fifo_wait_t time);

fifo_buf_t *fifo_recv(fifo_handle_t fifo_handle, fifo_wait_t time);

void fifo_buf_free(fifo_buf_t *buf);

void fifo_delete(fifo_handle_t fifo_handle);


#ifdef __cplusplus
}
#endif

#endif