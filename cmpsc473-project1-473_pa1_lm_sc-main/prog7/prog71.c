#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>

int main(int argc, char *argv[])
{
    printf("PID of prog71.c: %d\n",getpid());
    int rc = fork ();
    char* pc[16];
    memset(pc, 'A', sizeof(pc));    

    int *p = malloc(sizeof(int));
    assert(p!= NULL);
    printf("PID: %d, Address pointed by p: %p\n",getpid(),p);

    if (rc < 0) {
        fprintf(stderr, "fork failed\n");
        exit(1);
    }

    else if (rc == 0) {
        printf("Hello, We are in child process (process B), PID: %d\n",getpid());
        char *myargs[] = {"Hello","World",NULL};
        execv("./prog72", myargs);
        printf("this should not print\n");
    }

    else {
        sleep(10);
        printf("Hello, We are in parent process (process A), PID: %d\n",getpid());
        printf("Sending signal to process B (PID: %d)\n",rc);
        kill(rc,SIGTERM);
        sleep(1);
        printf("Welcome back to process A, PID: %d\n",getpid());
        }
    return 0;
}

