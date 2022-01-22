#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "check.h"

int x[5] = {1, 2, 3, 4, 5};

void allocate()
{
    int i;
    int *p;
    for(i =1; i<1000000; i++)
    {
        p = malloc(500 * sizeof(int));
        if (func(i)) { free(p); }
    }
}

void allocate1()
{
    int i;
    int *p;
    for (i=1; i<10000; i++)
    {
        p = malloc(1000 * sizeof(int));
        if (i & 1)
            free(p);
    }
}

void allocate2()
{
    int i;
    int *p;
    for (i=1; i<300000; i++)
    {
        p = malloc(10000 * sizeof(int));
        free(p);
    }
}

int main (int argc, char const *argv[])
{
    struct rusage usage;
    struct timeval userStart, userEnd, systemStart, systemEnd;

    int i;
    int *p;
    printf ("Executing the code ......\n") ;

    getrusage(RUSAGE_SELF, &usage);
    userStart = usage.ru_utime;
    systemStart = usage.ru_stime;

    allocate();

    for (i=0; i<10000; i++)
    {
        p = malloc(1000 * sizeof(int));
        free(p);
    }

    getrusage(RUSAGE_SELF, &usage);
    userEnd = usage.ru_utime;
    systemEnd = usage.ru_stime;

    printf("(i)   User CPU time used: %f ms\n", (userEnd.tv_sec - userStart.tv_sec) * 1000 + 1.0 * (userEnd.tv_usec - userStart.tv_usec) / 1000);
    printf("(ii)  System CPU time used: %f ms\n", (systemEnd.tv_sec - systemStart.tv_sec) * 1000 + 1.0 * (systemEnd.tv_usec - systemStart.tv_usec) / 1000);
    printf("(iii) Maximum resident set size: %ld KB\n", usage.ru_maxrss);
    printf("(iii) Signals Received: %ld\n", usage.ru_nsignals);
    printf("(iv)  Voluntary Context Switches: %ld\n", usage.ru_nvcsw);
    printf("(v)   Involuntary Context Switches: %ld\n", usage.ru_nivcsw);
    
    printf("Program execution successful\n");
    return 0 ;
}