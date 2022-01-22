#ifndef HELPER_H
#define HELPER_H
#include "queue.h"
//buffer structure. You are allowed to additional members in it. 
//    DO NOT CHANGE ANY EXISTING MEMBER
//    DO NOT CHANGE ANY EXISTING MEMBER


typedef struct {
    bool isopen;
    fifo_t* fifoQ;
    pthread_mutex_t chmutex;
    pthread_mutex_t chclose;
    pthread_cond_t chconrec;
    pthread_cond_t chconsend;
} state_t;
#endif 