
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include "buffer.h"
#include <semaphore.h>
#include <unistd.h>
#include <sys/resource.h>
//structs

#define mu_str_(text) #text
#define mu_str(text) mu_str_(text)
#define mu_assert(message, test) do { if (!(test)) return "FAILURE: See " __FILE__ " Line " mu_str(__LINE__) ": " message; } while (0)
#define mu_run_test(test) do { char *message = test(); tests_run++; \
                                if (message) return message; } while (0)

typedef struct
{
      int value;
      char* key;
      FILE* fileptr;
} intStrMap;

typedef struct {
    enum buffer_status out;
    sem_t *done;
    char* data;
} send_args;

typedef struct {
    enum buffer_status out;
    sem_t *done;
    void* data;
} recv_args;

//global variable
int gSender;
state_t* BUF ;
FILE * reducefileptr ;
int threadCount = 50; //default thread count
int gMapperThreads = 50;
int gBufferSize = 200; 

//prototypes
void tdestroy(void *root, void (*free_node)(void *nodep));
int compar(const void *pa, const void *pb);
void reformat_string(char *src, char *dst) ;
void dividefile(char* filename,int num);
void* thread_helper_send(send_args* myargs);
void* direct_send(send_args* myargs);
void wordcount(char * filename);
void Reducer();
void actionReduce(const void *nodep, VISIT which, int depth);
void deletefile(int num);
char* compareFiles(char* file1,char* file2);


void* direct_send(send_args* myargs)
{
    myargs->out = buffer_send(BUF,myargs->data);
    if (myargs->done) {
        sem_post(myargs->done);
    }
    return NULL;
}

typedef struct {
    long double data;
    pthread_t pid;
} cpu_args;

void* average_cpu_utilization(cpu_args* myargs) {

    struct rusage usage1;
    struct rusage usage2;

    getrusage(RUSAGE_SELF, &usage1);
    struct timeval start = usage1.ru_utime;
    struct timeval start_s = usage1.ru_stime;
    sleep(20);
    getrusage(RUSAGE_SELF, &usage2);
    struct timeval end = usage2.ru_utime;
    struct timeval end_s = usage2.ru_stime;
    
    long double result = (end.tv_sec - start.tv_sec)*1000000L + end.tv_usec - start.tv_usec + (end_s.tv_sec - start_s.tv_sec)*1000000L + end_s.tv_usec - start_s.tv_usec;
    
    myargs->data = result;
    return NULL;
}

int string_equal(const char* str1, const char* str2) {
    if ((str1 == NULL) && (str2 == NULL)) {
        return 1;
    }
    if ((str1 == NULL) || (str2 == NULL)) {
        return 0;
    }
    return (strcmp(str1, str2) == 0);
}

char * test_send_correctness() { // test_send_correctness


    BUF = buffer_create(25);
    void* out = calloc(sizeof(char),1024);
    pthread_t pid[3];
    sem_t send_done;
    sem_init(&send_done, 0, 0);

    send_args new_args;
    char* data = "PSU";  // size is 4 + 4 = 8 
    new_args.done = &send_done;
    new_args.data = data;
    pthread_create(&pid[0], NULL, (void *)direct_send, &new_args);

    sem_wait(&send_done);

    
    buffer_top_message(BUF,&out,1);

    mu_assert("test_send_correctness: Testing channel value failed", string_equal(out,"PSU"));
    mu_assert("test_send_correctness: Testing channel size failed" ,fifo_used_size(BUF->fifoQ)==8);
    mu_assert("test_send_correctness: Testing channel return failed",new_args.out ==BUFFER_SUCCESS);
    

    send_args new_args1;
    char* data1 = "CMPSC";  // size is 4 + 6 = 10
    new_args1.done = &send_done;
    new_args1.data = data1;
    pthread_create(&pid[1], NULL, (void *)direct_send, &new_args1);

    sem_wait(&send_done);
    
    mu_assert("test_send_correctness: Testing channel value failed", string_equal(out,"PSU"));
    buffer_top_message(BUF,&out,2);
    mu_assert("test_send_correctness: Testing channel value failed", string_equal(out,"CMPSC"));
    mu_assert("test_send_correctness: Testing buffer size failed" ,fifo_used_size(BUF->fifoQ)==18);
    mu_assert("test_send_correctness: Testing channel return failed",new_args1.out ==BUFFER_SUCCESS);
    
    send_args new_args2;
    char* data2 = "MoNdAy";  // size is 4 + 7 = 11 
    new_args2.done = &send_done;
    new_args2.data = data2;
    new_args2.out = BUFFER_ERROR;
    pthread_create(&pid[2], NULL, (void *)direct_send, &new_args2);
    
    usleep(10000);

    buffer_top_message(BUF,&out,1);
    mu_assert("test_send_correctness: Testing channel value failed", string_equal(out,"PSU"));
    buffer_top_message(BUF,&out,2);
    mu_assert("test_send_correctness: Testing channel value failed", string_equal(out,"CMPSC"));
    mu_assert("test_send_correctness: Testing channel size failed" ,fifo_used_size(BUF->fifoQ)==18);
    mu_assert("test_send_correctness: Testing channel return failed", new_args2.out ==BUFFER_ERROR);

    buffer_receive(BUF, &out);
    for (size_t i = 0; i < 3; i++) {
        pthread_join(pid[i], NULL);
    }

    buffer_receive(BUF, &out);
    buffer_receive(BUF, &out);

    free(out);
    buffer_close(BUF);
    sem_destroy(&send_done);
    buffer_destroy(BUF);
    return NULL;
}

void* direct_receive(recv_args* myargs) {
    myargs->out = buffer_receive(BUF, &myargs->data);
    if (myargs->done) {
        sem_post(myargs->done);
    }
    return NULL;
}

char * test_receive_correctness() { // test_recieve_correctness

    BUF = buffer_create(18);
    void* out = calloc(sizeof(char),1024);
    pthread_t pid[2];


    buffer_send(BUF, "five");  //4+5 = 9
    buffer_send(BUF, "PSU");   // 4+4 = 8
    mu_assert("test_receive_correctness: Testing channel size failed" ,fifo_used_size(BUF->fifoQ)==17);
    buffer_receive(BUF, &out);

    mu_assert("test_receive_correctness: Testing channel size failed" ,fifo_used_size(BUF->fifoQ)==8);
    mu_assert("test_receive_correctness: Testing channel value failed", string_equal(out,"five"));
    
    buffer_receive(BUF, &out);
    mu_assert("test_receive_correctness: Testing channel size failed" ,fifo_used_size(BUF->fifoQ)==0);
    mu_assert("test_receive_correctness: Testing channel value failed", string_equal(out,"PSU"));

    sem_t send_done;
    sem_init(&send_done, 0, 0);

    void* out1 = calloc(sizeof(char),1024);
    void* out2 = calloc(sizeof(char),1024);
    recv_args rec1,rec2;
    rec1.done = &send_done;
    rec1.data = out1;
    rec1.out = BUFFER_ERROR;
    pthread_create(&pid[0], NULL, (void *)direct_receive, &rec1);

    rec2.done = &send_done;
    rec2.data = out2;
    rec2.out = BUFFER_ERROR;
    pthread_create(&pid[1], NULL, (void *)direct_receive, &rec2);
    
    usleep(10000);

    mu_assert("test_receive_correctness: Testing channel size failed" ,fifo_used_size(BUF->fifoQ)==0);
    mu_assert("test_receive_correctness: Testing channel return failed", rec1.out ==BUFFER_ERROR);
    mu_assert("test_receive_correctness: Testing channel return failed", rec2.out ==BUFFER_ERROR);

    buffer_send(BUF, "five"); 
    sem_wait(&send_done);
    // five at top or second

    mu_assert("test_receive_correctness: Testing channel return failed",(rec1.out == BUFFER_SUCCESS || rec2.out == BUFFER_SUCCESS ));
    mu_assert("test_receive_correctness: Testing channel return failed",!(rec1.out == BUFFER_SUCCESS && rec2.out == BUFFER_SUCCESS ));
    
    
    buffer_send(BUF, "PSU");
    sem_wait(&send_done);
    mu_assert("test_receive_correctness: Testing channel return failed",rec1.out == BUFFER_SUCCESS && rec2.out == BUFFER_SUCCESS );
    // six at top or second

    for (size_t i = 0; i < 2; i++) {
        pthread_join(pid[i], NULL);
    }
    free(out);
    free(out1);
    free(out2);
    buffer_close(BUF);
    buffer_destroy(BUF);
    sem_destroy(&send_done);

    return NULL;
}

char * test_overall_send_receive() // overall send receive
{
    BUF = buffer_create(50);
    size_t RECEIVE_THREAD = 10;
    size_t SEND_THREAD = 10;
    void* out[RECEIVE_THREAD] ;
    for (size_t i = 0; i < RECEIVE_THREAD; i++)
        out[i]= calloc(sizeof(char),1024);

    
    pthread_t rec_pid[RECEIVE_THREAD];
    pthread_t send_pid[SEND_THREAD];

    recv_args rec[RECEIVE_THREAD];
    send_args send[SEND_THREAD];

    for (size_t i = 0; i < RECEIVE_THREAD; i++) {
        rec[i].data = out[i];
        rec[i].done = NULL;
        pthread_create(&rec_pid[i], NULL, (void *)direct_receive, &rec[i]);
    }

    for (size_t i = 0; i < SEND_THREAD; i++) {
        send[i].data = "cmpsc473";
        send[i].done = NULL;
        pthread_create(&send_pid[i], NULL, (void *)direct_send, &send[i]);  
    }

    for (size_t i = 0; i < RECEIVE_THREAD; i++) {
        pthread_join(rec_pid[i], NULL);
    }

    for (size_t i = 0; i < SEND_THREAD; i++){
        pthread_join(send_pid[i], NULL);
    }

    for (size_t i = 0; i < SEND_THREAD; i++) {
        mu_assert("test_overall_send_receive: Testing channel return failed", send[i].out == BUFFER_SUCCESS);
    }
    for (size_t i = 0; i < RECEIVE_THREAD; i++) {
        mu_assert("test_overall_send_receive: Testing channel return failed", rec[i].out ==BUFFER_SUCCESS);
        mu_assert("test_overall_send_receive: Testing channel value failed", string_equal( rec[i].data, "cmpsc473"));
    }

    for (size_t i = 0; i < RECEIVE_THREAD; i++)
        free(out[i]);

    buffer_close(BUF);
    buffer_destroy(BUF);
    return NULL;
}


char* test_channel_close_with_receive() {
    
    /*This test ensures that all the blocking calls should return with proper status 
     * when the channel gets closed 
     */

    // Testing for the close API 
    BUF = buffer_create(25);
    for (size_t i = 0; i < 2; i++) {
        buffer_send(BUF, "Message");
    }

    size_t RECEIVE_THREAD = 10;
    
    recv_args data_rec[RECEIVE_THREAD];
    pthread_t rec_pid[RECEIVE_THREAD];

    sem_t done;
    sem_init(&done, 0, 0);  
    void* out = calloc(sizeof(char),1024);
    for (size_t i = 0; i < RECEIVE_THREAD; i++) {
        data_rec[i].done = &done;
        data_rec[i].data = out;
        data_rec[i].out = BUFFER_ERROR;
        pthread_create(&rec_pid[i], NULL, (void *)direct_receive, &data_rec[i]); 
    }

    // Wait for receive threads to drain the buffer
    for (size_t i = 0; i < 2; i++) {
        sem_wait(&done);
    }

    // Close channel to stop the rest of the receive threads
    mu_assert("test_channel_close_with_receive: Testing channel return failed",buffer_close(BUF) == BUFFER_SUCCESS);

    // XXX: All calls should immediately after close else close implementation is incorrect.
    for (size_t i = 0; i < RECEIVE_THREAD; i++) {
        pthread_join(rec_pid[i], NULL); 
    }

    size_t count = 0;
    for (size_t i = 0; i < RECEIVE_THREAD; i++) {
        if (data_rec[i].out == CLOSED_ERROR) {
            count ++;
        }
    }

    mu_assert("test_channel_close_with_receive: Testing channel return failed", count == (RECEIVE_THREAD - 2));

    // Making a normal send call to check if its closed

    enum buffer_status res = buffer_receive(BUF, &out);
    mu_assert("test_channel_close_with_receive: Testing channel return failed", res == CLOSED_ERROR);
    
    
    mu_assert("test_channel_close_with_receive: Testing channel return failed", buffer_close(BUF) == CLOSED_ERROR);
    free(out);
    buffer_destroy(BUF);
    sem_destroy(&done);
    return NULL;
}

char* test_channel_close_with_send() {

    /*This test ensures that all the blocking calls should return with proper status 
     * when the channel gets closed 
    */

    // Testing for the send API 
    BUF= buffer_create(25);

    size_t SEND_THREAD = 10;
    
    pthread_t send_pid[SEND_THREAD];
    send_args data_send[SEND_THREAD];

    sem_t send_done;
    sem_init(&send_done, 0, 0);
    char* data1 = "Message";
    for (size_t i = 0; i < SEND_THREAD; i++) {
        data_send[i].done = &send_done;
        data_send[i].data = data1;
        data_send[i].out = BUFFER_ERROR;;
        pthread_create(&send_pid[i], NULL, (void *)direct_send, &data_send[i]);  
    }
    
    for (size_t i = 0; i < 2; i++) {
        sem_wait(&send_done);
    }
    mu_assert("test_channel_close_with_send: Testing channel return failed", buffer_close(BUF) == BUFFER_SUCCESS);

    // XXX: All the threads should return in finite amount of time else it will be in infinite loop. Hence incorrect implementation
    for (size_t i = 0; i < SEND_THREAD; i++) {
        pthread_join(send_pid[i], NULL);    
    }
    
    size_t count = 0;
    for (size_t i = 0; i < SEND_THREAD; i++) {
        if (data_send[i].out == CLOSED_ERROR) {
            count ++;
        }
    }

    mu_assert("test_channel_close_with_send: Testing channel return failed", count == (SEND_THREAD - 2));

    // Making a normal send call to check if its closed
    enum buffer_status out = buffer_send(BUF, "Message");
    mu_assert("test_channel_close_with_send: Testing channel return failed", out == CLOSED_ERROR);
    
    mu_assert("test_channel_close_with_send: Testing channel return failed", buffer_close(BUF) == CLOSED_ERROR);
    buffer_destroy(BUF);
    sem_destroy(&send_done);
    return NULL;

}
char * test_Free () {
    
    /* This test is to check if free memory fails if the channel is not closed */
    BUF = buffer_create(2);

    mu_assert("test_Free: Testing channel return failed", buffer_destroy(BUF) == DESTROY_ERROR);
    mu_assert("test_Free: Testing channel return failed",buffer_close(BUF) == BUFFER_SUCCESS);
    mu_assert("test_Free: Testing channel return failed", buffer_destroy(BUF) == BUFFER_SUCCESS);
    return NULL;
    
}

char * test_for_too_many_wakeups() //cpu
{

    /* This test checks the CPU utilization of all the send, receive , select APIs.
     */
    size_t THREADS = 100;
    pthread_t pid[THREADS];
    recv_args args[THREADS];
    BUF = buffer_create(12);
    void* out1 = calloc(sizeof(char),1024);
    sem_t done;
    sem_init(&done, 0, 0);

    for (size_t i = 0; i < THREADS; i++) {
        args[i].out = BUFFER_ERROR;
        args[i].done = &done;
        args[i].data = out1;
        pthread_create(&pid[i], NULL, (void *)direct_receive, &args[i]);
    }

    sleep(2);

    struct rusage usage1;
    getrusage(RUSAGE_SELF, &usage1);
    struct timeval start = usage1.ru_utime;
    struct timeval start_s = usage1.ru_stime;

    for (size_t i = 0; i < THREADS; i++) {
        buffer_send(BUF, "OS473");
        sem_wait(&done);
        usleep(10000);
    }

    struct rusage usage2;
    getrusage(RUSAGE_SELF, &usage2);
    struct timeval end = usage2.ru_utime;
    struct timeval end_s = usage2.ru_stime;

    long double result = (end.tv_sec - start.tv_sec)*1000000L + end.tv_usec - start.tv_usec + (end_s.tv_sec - start_s.tv_sec)*1000000L + end_s.tv_usec - start_s.tv_usec;
    mu_assert("test_for_too_many_wakeups: Testing channel value failed",result < 200000);
    
    for (size_t i = 0; i < THREADS; i++) {
        pthread_join(pid[i], NULL);
    }

    free(out1);
    buffer_close(BUF);
    buffer_destroy(BUF);
    sem_destroy(&done);
    return NULL;
    
}
char * serialize ()
{
    
    BUF = buffer_create(1024);
    wordcount("testinput.txt");
    reducefileptr = fopen("testoutput.txt","w");

    if(!reducefileptr)
    {
        printf("unable to create file low memory ?\n");
        exit(0);
    }
    Reducer();
    buffer_close(BUF);
    buffer_destroy(BUF);
    fclose(reducefileptr);
    char* com_result = compareFiles("testoutput.txt","correct_testoutput.txt");
    mu_assert("serialize: Testing channel value failed", string_equal(com_result,""));
    free(com_result);
    return NULL;
}

char * test_cpu_utilization_send()
{
    /* This test checks the CPU utilization of all the send APIs.
     */
    // For SEND API
    size_t THREADS = 100;
    BUF = buffer_create(17);
    pthread_t cpu_pid, pid[THREADS];

    // Fill buffer
    
    buffer_send(BUF, "PSU"); //size is 4 + 4 = 8
    buffer_send(BUF, "PSU"); //size is 4 + 4 = 8
    
    send_args data_send[THREADS];
    for (int i = 0; i < THREADS; i++) {
        data_send[i].data = "PSU" ;  
        data_send[i].done = NULL ;
        pthread_create(&pid[i], NULL, (void *)direct_send, &data_send[i]);
    }
    
    sleep(5);

    cpu_args args;
    pthread_create(&cpu_pid, NULL, (void *)average_cpu_utilization, &args);  
    sleep(20);
    pthread_join(cpu_pid, NULL);

    for (int i = 0; i < THREADS; i++) {
        void* data = calloc(sizeof(char),1024);
        buffer_receive(BUF, &data);
        mu_assert("test_cpu_utilization_send: Testing channel value failed", string_equal(data, "PSU"));
        free(data);
    }
    
    for (int i = 0; i < THREADS; i++) {
        pthread_join(pid[i], NULL);
    }

    mu_assert("test_cpu_utilization_send: Testing channel value failed", args.data < 50000);

    buffer_close(BUF);
    buffer_destroy(BUF);
    return NULL;
}



char * test_cpu_utilization_receive() {
    /* This test checks the CPU utilization of all the receive APIs.
     */
    // For RECEIVE API
    size_t THREADS = 100;
    BUF= buffer_create(25);
    pthread_t cpu_pid, pid[THREADS];

    recv_args data_receive[THREADS];
    for (int i = 0; i < THREADS; i++) {
        data_receive[i].done = NULL;
        data_receive[i].data =  calloc(sizeof(char),1024);
        pthread_create(&pid[i], NULL, (void *)direct_receive, &data_receive[i]);
    }

    sleep(5);

    cpu_args args;
    pthread_create(&cpu_pid, NULL, (void *)average_cpu_utilization, &args);

    sleep(20);
    pthread_join(cpu_pid, NULL);

    for (int i = 0; i < THREADS; i++) {
        buffer_send(BUF, "Message");
    }
    for (int i = 0; i < THREADS; i++) {
        pthread_join(pid[i], NULL);
        mu_assert("test_cpu_utilization_receive: Testing channel value failed", string_equal(data_receive[i].data, "Message"));
    }
    mu_assert("test_cpu_utilization_receive: Testing channel value failed",args.data < 50000);
    
    for (int i = 0; i < THREADS; i++) 
        free(data_receive[i].data );

    buffer_close(BUF);
    buffer_destroy(BUF);
    return NULL;

}

char* compareFiles(char* file1,char* file2)
{
    FILE *fp;
    int status;
    char* result = (char*)calloc(sizeof(char),256);
    char diffCMD[256];
    sprintf(diffCMD,"diff %s %s",file1,file2);
    fp = popen(diffCMD, "r");

    if (fp == NULL)
        return NULL;

    fgets(result, 256, fp);

    status = pclose(fp);
    if (status == -1) {
        printf(" Error reported by pclose() ");
        return NULL;
    }
    return result;
}

char * test_correctness() { 
    char * inputFile = "input.txt";
    reducefileptr = fopen("output.txt","w");
    if(!reducefileptr)
    {
        printf("unable to create file low memory ?\n");
        exit(0);
    }
    BUF = buffer_create(100);
    pthread_t pid[threadCount], reducePID;
    
    dividefile(inputFile,threadCount);
    send_args new_args[threadCount];
    
    char* fname[threadCount];
    for( int i=0 ;i<threadCount; i++)
    {
        fname[i] = (char *)calloc(sizeof(char), 64);
        sprintf(fname[i], "tmp%d.txt", i);
        new_args[i].data= fname[i];
        pthread_create(&pid[i], NULL, (void *)thread_helper_send, &new_args[i]); 
    }
    
    pthread_create(&reducePID, NULL, (void *)Reducer, NULL);

    for( int i=0 ;i<threadCount; i++) 
    {
        pthread_join(pid[i], NULL);
    }

    buffer_send(BUF,"splmsg");

    pthread_join(reducePID, NULL);

    for( int i=0 ;i<threadCount; i++)
        free(fname[i]);

    buffer_close(BUF);
    buffer_destroy(BUF);
    fclose(reducefileptr);
    deletefile(threadCount);
    char* com_result = compareFiles("output.txt","correct_output.txt");
    mu_assert("test_correctness: Testing channel value failed", string_equal(com_result,""));
    free(com_result);
    return NULL;
}


char * custom_eval(char* infile_name,char* outfile_name) { 
    
    printf( " custom_eval with %d threads and %d buffer size\n", gMapperThreads, gBufferSize);
    BUF = buffer_create(gBufferSize);
    pthread_t pid[gMapperThreads], reducePID;
    dividefile(infile_name,gMapperThreads);
    reducefileptr = fopen(outfile_name,"w");
    if(!reducefileptr)
    {
        printf("unable to create file low memory ?\n");
        exit(0);
    }
    send_args new_args[gMapperThreads];
    char* fname[gMapperThreads];

    for( int i=0 ;i<gMapperThreads; i++)
    {
        fname[i] = (char *)calloc(sizeof(char), 64);
        sprintf(fname[i], "tmp%d.txt", i);
        new_args[i].data= fname[i];
        pthread_create(&pid[i], NULL, (void *)thread_helper_send, &new_args[i]); 
    }
    usleep(10000);
    pthread_create(&reducePID, NULL, (void *)Reducer, NULL);

    for( int i=0 ;i<gMapperThreads; i++) 
    {
        pthread_join(pid[i], NULL);
    }

    buffer_send(BUF,"splmsg");

    pthread_join(reducePID, NULL);

    for( int i=0 ;i<gMapperThreads; i++)
        free(fname[i]);


    buffer_close(BUF);
    buffer_destroy(BUF);
    fclose(reducefileptr);
    deletefile(gMapperThreads);
    return NULL;
}

char* test_initialization() {

    /* In this part of code we are creating a channel and checking if its intialization is correct */
    size_t capacity = 10000;
    BUF = NULL;
    BUF = buffer_create(capacity);

    mu_assert("test_initialization: Could not create channel\n",BUF != NULL);
    mu_assert("test_initialization: Did not create buffer\n",BUF->fifoQ != NULL);
    mu_assert("test_initialization: Buffer size is not as expected\n",fifo_used_size(BUF->fifoQ) ==  0);   
    mu_assert("test_initialization: Buffer capacity is not as expected\n",BUF->fifoQ->size ==  capacity);
    
    buffer_close(BUF);
    buffer_destroy(BUF);
    return NULL;
}

typedef char* (*test_fn_t)();
typedef struct {
    char* name;
    test_fn_t test;
} test_t;

test_t tests[] = {{"test_initialization", test_initialization},
                  {"test_send_correctness", test_send_correctness},
                  {"test_receive_correctness", test_receive_correctness},
                  {"test_overall_send_receive", test_overall_send_receive},
                  {"test_for_too_many_wakeups", test_for_too_many_wakeups},
                  {"test_cpu_utilization_send", test_cpu_utilization_send},
                  {"test_cpu_utilization_receive", test_cpu_utilization_receive},
                  {"test_channel_close_with_receive", test_channel_close_with_receive},
                  {"test_channel_close_with_send", test_channel_close_with_send},
                  {"test_Free", test_Free},
                  {"serialize", serialize},
                  {"test_correctness", test_correctness},
                  
};

size_t num_tests = sizeof(tests)/sizeof(tests[0]);

int tests_run = 0;
int tests_passed = 0;

char* single_test(test_fn_t test, size_t iters) {
    for (size_t i = 0; i < iters; i++) {
        mu_run_test(test);
    }
    return NULL;
}


char* all_tests(size_t iters) {
    for (size_t i = 0; i < num_tests; i++) {
        char* result = single_test(tests[i].test, iters);
        if (result != NULL) {
            return result;
        }
    }
    return NULL;
}




int main(int argc, char** argv) {

    if(argc == 6 && (strcmp(argv[1],"custom_eval")==0) )   //performance test  [ usage : ./driver custom_eval #ofthread bufferSize inputfile outputfile ]
    {
        gMapperThreads = atoi(argv[2]);
        gBufferSize = atoi(argv[3]);
        char * infile_name = argv[4];
        char * outfile_name = argv[5];
        custom_eval(infile_name,outfile_name);
        exit(0);
    }
    if(argc >1 && (strcmp(argv[1],"custom_eval")==0) )   //performance test  [ usage : ./driver custom_eval #ofthread bufferSize inputfile outputfile ]
    {
        if(argc != 6)
        printf( "custom_eval  usage : ./driver custom_eval #ofthread bufferSize inputfile outputfile \n");
        exit(0);
    }
    char* result = NULL;
    size_t iters = 1;
    if (argc == 1) {
        result = all_tests(iters);
        if (result != NULL) {
            printf("%s\n", result);
        } else {
            printf("ALL TESTS PASSED\n");
        }

        printf("Tests run: %d\n", tests_run);
 
        return result != NULL;
    } else if (argc == 3) {
        iters = (size_t)atoi(argv[2]);
    } else if (argc > 3) {
        printf("Wrong number of arguments, only one test is accepted at time");
    }

    result = "Did not find test";

    for (size_t i = 0; i < num_tests; i++) {
        if (string_equal(argv[1], tests[i].name)) {
            result = single_test(tests[i].test, iters);
            break;
        }
    }
    if (result) {
        printf("%s\n", result);
    }
    else {
        printf("ALL TESTS PASSED\n");
    }

    printf("Tests run: %d\n", tests_run);

    return result != NULL;
}

void* thread_helper_send(send_args* myargs) {
    wordcount(myargs->data);
    return NULL;
}


void wordcount(char * filename)
{
    char* wordKV;
    FILE *fptr = fopen(filename,"r");
    if(!fptr)
        return ;
    
    char x[1024];
    char y[1024];
    /* assumes no word exceeds length of 1023 */
    while (fscanf(fptr, " %1023s", x) == 1) {
        reformat_string(x,y);
        intStrMap *a = malloc(sizeof(intStrMap));
        a->key = strdup(y);
        a->value = 1;
        wordKV = (char *)calloc(sizeof(char), 64);
        if(!strcmp(a->key,"splmsg"))
            sprintf(wordKV,"%s", a->key);
        else
            sprintf(wordKV,"%s %d", a->key,a->value);
        buffer_send(BUF,wordKV);
        free(wordKV);
        free(a->key);
        free(a);
    }
    fclose(fptr);
    return ;
}

// divide the givel file in given num
void dividefile(char* filename,int num)
{
    int res = 0;
    if(num ==1)
    {
        char fname[64];
        char command_cp[64];
        sprintf(fname, "tmp%d.txt", 0);
        sprintf(command_cp, "cp -r %s %s", filename,fname);
        system(command_cp);
        return;
    }
    FILE* fp = fopen(filename,"r");
    if(!fp)
    {
        printf("unable to open %s file\n",filename);
        exit(0);
    }
    FILE* fpa[num];
    fseek(fp, 0L, SEEK_END);
    long int sz = ftell(fp);
    int number = (int)(sz/num) ; 
    rewind(fp);
    int ch =-1;
    char fname[64];
    int i;
    for( i=0 ;i< (num-1); i++)
    {
        sprintf(fname, "tmp%d.txt", i);
        fpa[i] = fopen(fname,"w");
        if(!fpa[i])
        {
            printf("unable to create file low memory ?\n");
            exit(0);
        }
        for(int j = 0;j<number;j++)
        {
            ch = fgetc(fp);
            if(ch==-1)
                break;
            res = fputc(ch,fpa[i]);
            if(res == EOF)
            {
                printf("unable to write to file low memory ?\n");
                exit(0);
            }
        }
        ch = fgetc(fp);
        while(ch!=' ' && ch!='\n' && ch!=-1)
        {
            res = fputc(ch,fpa[i]);
            if(res == EOF)
            {
                printf("unable to write to file low memory ?\n");
                exit(0);
            }
            ch = fgetc(fp);   
        }
        fclose( fpa[i] );
    }
    sprintf(fname, "tmp%d.txt", i);
    fpa[i] = fopen(fname,"w");
    if(!fpa[i])
    {
        printf("unable to create file low memory ?\n");
        exit(0);
    }

    while (ch != -1)
    {
        res = fputc(ch,fpa[i]);
        if(res == EOF)
        {
            printf("unable to write to file low memory ?\n");
            exit(0);
        }
        ch = fgetc(fp);
    }
    fclose( fpa[i] );
    fclose(fp);

}

void reformat_string(char *src, char *dst) {
    for (; *src; ++src)
        if (!ispunct((unsigned char) *src))
            *dst++ = (char)tolower((unsigned char) *src);
    *dst = 0;
}

int compar(const void *pa, const void *pb)
{
    return strcmp( ( *(intStrMap *)pa).key ,(*(intStrMap *) pb).key );
}

void free_node(void *nodep)
{
    free(((intStrMap *)nodep)->key);
    free(nodep);
}

void Reducer()
{
    void* out = calloc(sizeof(char),1024);
    // receive 
    void* rooteduce = 0;
    while( buffer_receive(BUF, &out) ==1 )
    {  
        //printf("received output is %s\n", (char*)out);
        char * key = strtok(out," ");
        char* val = (strtok(NULL," "));
        int value;
        if(val)
             value = atoi(val);
        else
             value = -1;

        intStrMap *a = malloc(sizeof(intStrMap));
        a->key = strdup(key);
        a->value = value;
    
        intStrMap ** tmp =  tfind(a, &rooteduce, compar);
        if (!tmp ) // not found
        {
            tsearch(a, &rooteduce, compar);
        }
        else    // iteam found
        {
            (*tmp)->value += value;
            free(a->key);
            free(a);
        }

    }
    free(out);
    twalk(rooteduce, actionReduce);
    tdestroy(rooteduce, free_node);
}



void actionReduce(const void *nodep, VISIT which, int depth)
{
    intStrMap *datap;

    switch (which) {
    case preorder:
        break;
    case postorder:
        datap = *(intStrMap **) nodep;
        fprintf(reducefileptr,"%s %d\n",datap->key,datap->value);
        break;
    case endorder:
        break;
    case leaf:
        datap = *(intStrMap **) nodep;
        fprintf(reducefileptr,"%s %d\n",datap->key,datap->value);
        break;
    }
}

// deletes temp files
void deletefile(int num)
{
    char fname[64];
    for(int i=0 ;i<num; i++)
    {
        sprintf(fname, "tmp%d.txt", i);
        remove(fname);
    }
}