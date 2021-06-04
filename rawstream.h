#ifndef _RAWSTREAM_H_
#define _RAWSTREAM_H_

#include <stdint.h>

typedef void* rawstream_d;
typedef uint32_t rawstream_dev_id;

rawstream_d rawstream_open(rawstream_dev_id id, size_t dev_buffer_size, void (*on_new_data_callback)(rawstream_dev_id));
size_t rawstream_read(rawstream_d rd, uint8_t *buff, size_t count);
size_t rawstream_write(rawstream_d rd, uint8_t *buff, size_t count);

#endif //_RAWSTREAM_H_