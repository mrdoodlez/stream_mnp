/*!
 * \brief Raw stream ring buffer module with locked read & write ops
 * \author   Stan Raskov a.k.a mrdoodlez
 */

#include "rawstream.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>  //TODO: remove

typedef struct rawstream_buffer {
    uint8_t *buff;
    size_t len;
    size_t count;
    size_t rpos;
    size_t wpos;
    pthread_mutex_t mutex;
} rawstream_buffer_t;

typedef struct rawstream_device {
    rawstream_dev_id id;
    rawstream_buffer_t *rb;
    void (*on_new_data_callback)(rawstream_dev_id);
} rawstream_device_t;

/***********************************************************************************************/

static rawstream_buffer_t* _rawstream_buffer_create(size_t size);
static size_t _rawstream_buffer_atomic_read (rawstream_buffer_t *rb, uint8_t *buff, size_t count);
static size_t _rawstream_buffer_atomic_write(rawstream_buffer_t *rb, uint8_t *buff, size_t count);

/***********************************************************************************************/

/*!
 * \brief Open stream port and register callback
 * @param id stream id
 * @param dev_buffer_size ringbuffer size
 * @param on_new_data_callback  new data doorbell callback
 * @return device handle
 */

rawstream_d rawstream_open(rawstream_dev_id id, size_t dev_buffer_size, void (*on_new_data_callback)(rawstream_dev_id)) {
    rawstream_buffer *rb = _rawstream_buffer_create(dev_buffer_size);
    if (rb == NULL) {
        return 0;
    }

    rawstream_device_t *dev = (rawstream_device_t *)malloc(sizeof(rawstream_device_t));
    if (dev == NULL) {
        return (rawstream_d)0;
    }

    dev->id = id;
    dev->rb = rb;
    dev->on_new_data_callback = on_new_data_callback;

    return (rawstream_d)dev;
}

/*!
 * \brief Read from stream wrapper
 * @param rd Stream handle
 * @param buff User-allocated buff
 * @param count Bytes to read
 * @return Bytes actually read
 */

size_t rawstream_read(rawstream_d rd, uint8_t *buff, size_t count) {
    if (rd == 0) {
        //TODO: set error status
        return 0;
    }
    rawstream_device_t *dev = (rawstream_device_t *)rd;
    //printf("[RAWSTREAM][rd] dev: %d, cnt: %d\n", dev->id, (int)count);
    return _rawstream_buffer_atomic_read(dev->rb, buff, count);
}

/*!
 * \brief Write to stream wrapper. Also notifies reader with callback
 * @param rd Stream handle
 * @param buff User-allocated buff
 * @param count Bytes to write
 * @return Bytes actually written
 */

size_t rawstream_write(rawstream_d rd, uint8_t *buff, size_t count) {
    if (rd == 0) {
        //TODO: set error status
        return 0;
    }
    rawstream_device_t *dev = (rawstream_device_t *)rd;
    //printf("[RAWSTREAM][wr] dev: %d, cnt: %d\n", dev->id, (int)count);
    size_t result = _rawstream_buffer_atomic_write(dev->rb, buff, count);
    if (dev->on_new_data_callback != NULL) {
        dev->on_new_data_callback(dev->id);
    }
    return result;
}

/***********************************************************************************************/

static rawstream_buffer_t* _rawstream_buffer_create(size_t size) {
    uint8_t *buff = (uint8_t*)malloc(size);
    if (buff == 0) {
        return NULL;
    }

    rawstream_buffer_t *rb = (rawstream_buffer_t*)malloc(sizeof(rawstream_buffer_t));
    if (rb == NULL) {
        free(buff);
        return NULL;
    }

    memset(rb, 0, sizeof(rawstream_buffer_t));

    rb->buff = buff;
    rb->len = size;
    rb->mutex = PTHREAD_MUTEX_INITIALIZER;

    return rb;
}

static size_t _rawstream_buffer_atomic_read (rawstream_buffer_t *rb, uint8_t *buff, size_t count) {
    size_t result = 0;
    pthread_mutex_lock(&rb->mutex);
    while(count && rb->count) {
        *buff = rb->buff[rb->rpos];
        rb->rpos = (rb->rpos + 1) % rb->len;
        rb->count--;
        count--;
        buff++;
        result++;
    }
    pthread_mutex_unlock(&rb->mutex);
    return result;
}

static size_t _rawstream_buffer_atomic_write(rawstream_buffer_t *rb, uint8_t *buff, size_t count) {
    pthread_mutex_lock(&rb->mutex);
    size_t result = count;
    while (count) {
        if (rb->count == rb->len) {
            //TODO: handle overrun
            printf("[RAWSTREAM][wr][err] overrun");
        }
        rb->buff[rb->wpos] = *buff;
        rb->wpos = (rb->wpos + 1) % rb->len;
        rb->count = (rb->count < rb->len) ? (rb->count + 1) : rb->len;
        buff++;
        count--;
    }
    pthread_mutex_unlock(&rb->mutex);
    return result;
}