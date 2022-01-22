#include <pthread.h>
typedef struct {
     char * buf;
     int head;
     int tail;
     int size;
     int avilSize;
     pthread_mutex_t Qlock;
} fifo_t;
void fifo_init(fifo_t * f, int size);
int fifo_write(fifo_t * f, const void * buf, int nbytes);
int fifo_read(fifo_t * f, void * buf, int nbytes);
int fifo_used_size(fifo_t * f);
void fifo_free(fifo_t * f);
int fifo_avail_size(fifo_t * f);