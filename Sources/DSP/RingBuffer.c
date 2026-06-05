#include "RingBuffer.h"
#include <stdatomic.h>
#include <stdlib.h>

struct RingBuffer {
    int                  capacity;
    int                  mask;       // capacity - 1
    float               *data;
    _Atomic unsigned int write_pos;  // written by producer, read by consumer
    _Atomic unsigned int read_pos;   // written by consumer, read by producer
};

RingBuffer *rb_create(int capacity) {
    if (capacity <= 0 || (capacity & (capacity - 1)) != 0) return NULL;

    RingBuffer *buf = calloc(1, sizeof(RingBuffer));
    if (!buf) return NULL;

    buf->data = malloc((size_t)capacity * sizeof(float));
    if (!buf->data) { free(buf); return NULL; }

    buf->capacity  = capacity;
    buf->mask      = capacity - 1;
    atomic_init(&buf->write_pos, 0u);
    atomic_init(&buf->read_pos,  0u);
    return buf;
}

void rb_destroy(RingBuffer *buf) {
    if (!buf) return;
    free(buf->data);
    free(buf);
}

int rb_write(RingBuffer *buf, const float *samples, int count) {
    unsigned write = atomic_load_explicit(&buf->write_pos, memory_order_relaxed);
    unsigned read  = atomic_load_explicit(&buf->read_pos,  memory_order_acquire);
    int space = buf->capacity - (int)(write - read);
    int n     = count < space ? count : space;
    for (int i = 0; i < n; i++) {
        buf->data[(write + (unsigned)i) & (unsigned)buf->mask] = samples[i];
    }
    atomic_store_explicit(&buf->write_pos, write + (unsigned)n, memory_order_release);
    return n;
}

int rb_read(RingBuffer *buf, float *samples, int count) {
    unsigned read  = atomic_load_explicit(&buf->read_pos,  memory_order_relaxed);
    unsigned write = atomic_load_explicit(&buf->write_pos, memory_order_acquire);
    int avail = (int)(write - read);
    int n     = count < avail ? count : avail;
    for (int i = 0; i < n; i++) {
        samples[i] = buf->data[(read + (unsigned)i) & (unsigned)buf->mask];
    }
    atomic_store_explicit(&buf->read_pos, read + (unsigned)n, memory_order_release);
    return n;
}

int rb_available_to_read(const RingBuffer *buf) {
    unsigned read  = atomic_load_explicit(&buf->read_pos,  memory_order_relaxed);
    unsigned write = atomic_load_explicit(&buf->write_pos, memory_order_acquire);
    return (int)(write - read);
}
