#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

void term_hdl2 (int signum)
{
    printf("Hello, We are in prog72's signal handler , PID: %d\n", getpid());
    printf("Signal Received from process A, and process B is killed \n");
    exit(1);
}

int main(int argc, char *argv[]) {
    printf("Hello, We are in prog72.c , PID: %d\n", getpid());
    int *p = malloc(sizeof(int));
    char* pc2[16];
    memset(pc2, 'A', sizeof(pc2));
    assert(p!= NULL);
    printf("PID: %d, Address pointed by p: %p\n",getpid(),p);

struct sigaction new_act;
memset(&new_act,'\0',sizeof(new_act));

new_act.sa_handler = term_hdl2;
sigemptyset (&new_act.sa_mask);
new_act.sa_flags= SA_NODEFER;

sigaction(SIGTERM,&new_act,NULL);

while (1) {
    sleep(1);
}
return 0;
}

