#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RingBuffer RingBuffer;

/// Create a lock-free SPSC ring buffer. Capacity must be a power of two.
RingBuffer *rb_create(int capacity);
void        rb_destroy(RingBuffer *buf);

/// Producer side (audio thread). Returns number of samples written.
int rb_write(RingBuffer *buf, const float *samples, int count);

/// Consumer side (DSP thread). Returns number of samples read.
int rb_read(RingBuffer *buf, float *samples, int count);

/// Samples available to read without blocking.
int rb_available_to_read(const RingBuffer *buf);

#ifdef __cplusplus
}
#endif
