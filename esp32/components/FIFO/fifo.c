#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "fifo.h"

int fifo_send(fifo_handle_t *fifo, uint8_t *data, int len)
{
    fifo_buf_t *head = NULL;

    if (NULL == fifo->ptr) {
        fifo->ptr = (fifo_buf_t *)malloc(sizeof(fifo_buf_t));
        head = fifo->ptr;
    } else {
        head = fifo->ptr;
        while (NULL != head->next) {
            head = head->next;
        }
        head->next = (fifo_buf_t *)malloc(sizeof(fifo_buf_t));
        head = head->next;
    }
    memset(head, 0, sizeof(fifo_buf_t));
    head->data = (uint8_t *)malloc(sizeof(uint8_t)*len);
    head->len = len;
    fifo_memcpy(head->data, data, len);

    return 1;
}

int fifo_recv(fifo_handle_t *fifo, uint8_t *data)
{
    fifo_buf_t *head = fifo->ptr;
    uint32_t len = 0;

    if (NULL != head) {
        fifo_memcpy(data, head->data, head->len);
        len = head->len;
        fifo->ptr = head->next;
        free(head->data);
        free(head);
    }

    return len;
}