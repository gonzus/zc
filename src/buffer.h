#ifndef ZMQ_HH_
#define ZMQ_HH_

typedef struct Buffer {
    int used;
    char* data;
} Buffer;

void buffer_init(int v);
int buffer_alloc(char** data);
void buffer_free(int pos);
void buffer_clean(void);

#endif
