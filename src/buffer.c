#include <stdio.h>
#include <stdlib.h>
#include "buffer.h"

#define BUFFER_BLOCK 10
#define MAX_DATA 1024

static int sbuf;
static Buffer* buffer;
static int verbose_;

static void buffer_enlarge(void);

void buffer_init(int v)
{
    verbose_ = v;
}

int buffer_alloc(char** data)
{
    int pos = -1;
    while (1) {
        int j;
        for (j = 0; j < sbuf; ++j) {
            if (buffer[j].used) {
                continue;
            }

            buffer[j].used = 1;
            if (buffer[j].data == 0) {
                if (verbose_) {
                    fprintf(stderr, "Allocating %d bytes for buffer %d\n",
                            MAX_DATA, j);
                }
                buffer[j].data = malloc(MAX_DATA);
            }
            pos = j;
            break;
        }

        if (pos >= 0) {
            break;
        }
        buffer_enlarge();
    }

    *data = buffer[pos].data;
    if (verbose_) {
        fprintf(stderr, "Will use buffer %d:%p\n", pos, data);
    }
    return pos;
}

void buffer_free(int pos)
{
    if (pos < 0 || pos >= sbuf) {
        printf("Cannot free buffer #%d, slots are [0..%d]\n", pos, sbuf-1);
        return;
    }

    buffer[pos].used = 0;
}

void buffer_clean(void)
{
    int j;
    for (j = 0; j < sbuf; ++j) {
        if (buffer[j].data == 0) {
            continue;
        }

        if (verbose_) {
            fprintf(stderr, "Freeing %d bytes in buffer #%d:%p\n",
                    MAX_DATA, j, buffer[j].data);
            if (buffer[j].used) {
                fprintf(stderr, "Buffer #%d was in use when cleaning up\n", j);
            }
        }
        free(buffer[j].data);
        buffer[j].data = 0;
        buffer[j].used = 0;
    }

    if (verbose_) {
        fprintf(stderr, "Freeing %d buffer slots\n",
                sbuf);
    }
    free(buffer);

    buffer = 0;
    sbuf = 0;
}

static void buffer_enlarge(void)
{
    int j;
    int s = sbuf + BUFFER_BLOCK;
    Buffer* b;

    if (verbose_) {
        fprintf(stderr, "Enlarging buffer slots from %d to %d\n",
                sbuf, s);
    }
    b = (Buffer*) calloc(s, sizeof(Buffer));

    for (j = 0; j < sbuf; ++j) {
        b[j] = buffer[j];
    }

    if (buffer != 0) {
        if (verbose_) {
            fprintf(stderr, "Deleting old %d slots\n", sbuf);
        }
        free(buffer);
    }

    buffer = b;
    sbuf = s;
}
