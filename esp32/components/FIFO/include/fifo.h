#ifndef __FIFO_H__
#define __FIFO_H__

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define fifo_memcpy memcpy

typedef struct fifo_buf{
    uint8_t *data;
    int len;
    struct fifo_buf *next;
} fifo_buf_t;

typedef struct {
    fifo_buf_t *ptr;
} fifo_handle_t;

int fifo_send(fifo_handle_t *fifo, uint8_t *data, int len);

int fifo_recv(fifo_handle_t *fifo, uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif