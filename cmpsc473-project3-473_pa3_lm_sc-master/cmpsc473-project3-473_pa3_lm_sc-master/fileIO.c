#include <string.h>
#include <inttypes.h>

#include "fileIO.h"

/* Opens the given input file and return the input file object
 *
 * Input:  traceName -- the name of the input file (of the form inputx.txt)
 * Output: fptr -- the input file object
 */
FILE* openTrace(char* traceName){
    FILE *fptr;
    char* foldername = "traces/";

    //Get the file path
    char* filename = malloc(sizeof(foldername) + sizeof(traceName) + 1 ); /* make space for the new string (should check the return value ...) */
    strcpy(filename, foldername); /* copy name into the new var */
    strcat(filename, traceName); /* add the extension */

    //Check if successfully open
    if ((fptr = fopen(filename,"r")) == NULL){
        printf("Error opening file %s.\n", filename);
        // Program exits if the file pointer returns NULL.
        exit(1);
    }

    return fptr;
}

int closeTrace(FILE* fptr){
    if(fptr)
    {
        return fclose(fptr);
        return 1;
    }
    return 0;
}

void fprintStats(gll_t* list, FILE* f)
{
    struct Stats* s;
    gll_node_t* currNode = list->first;
    while ( currNode!=NULL )
    {
        s = currNode->data;
        fprintf(f, "\n\nProcess: %s: \n", s->processName);
        fprintf(f, "Completion time: %llu\n", s->duration );
        fprintf(f, "Number of context switches = %d \n", s->numberOfContextSwitch );
        fprintf(f, "Number of disk interrupts = %d \n", s->numberOfDiskInt );
        fprintf(f, "Number of TLB misses = %d \n", s->numberOfTLBmiss );
        fprintf(f, "%% of TLB misses = %lf \n", 100.0 * s->numberOfTLBmiss / (s->numberOfTLBhit + s->numberOfTLBmiss) );
        fprintf(f, "Number of page faults = %d \n", s->missCount );
        fprintf(f, "%% of page faults = %lf \n", 100.0 * s->missCount / (s->hitCount + s->missCount) );
        fprintf(f, "Blocked state duration = %llu \n", s->blockedStateDuration );
        fprintf(f, "Amount of time spent in OS mode = %llu \n", s->OS_time );
        fprintf(f, "Amount of time spent in user mode = %llu \n", s->user_time );
        currNode = currNode->next;
    }
}

int writeToFile(char* filename, struct TotalStats resultStats){
    FILE* fptr;
    if ((fptr = fopen(filename,"w")) == NULL){
        printf("Error opening file %s.\n", filename);
        // Program exits if the file pointer returns NULL.
        return 0;
    }
    fprintf(fptr, "Start time = %llu, \tEnd time =%llu\n", resultStats.start_time, resultStats.end_time);
    fprintf(fptr, "Total number of context switches = %d \n", resultStats.numberOfContextSwitch );
    fprintf(fptr, "Total number of disk interrupts = %d \n", resultStats.numberOfDiskInt );
    fprintf(fptr, "Total number of TLB misses = %d \n", resultStats.totalTLBmiss );
    fprintf(fptr, "%% of TLB misses = %lf \n", 100.0 * resultStats.totalTLBmiss / (resultStats.totalTLBhit + resultStats.totalTLBmiss) );
    fprintf(fptr, "Total number of page faults = %d \n", resultStats.totalPgFaults );
    fprintf(fptr, "%% of page faults = %lf \n", 100.0 * resultStats.totalPgFaults / (resultStats.totalPgHits + resultStats.totalPgFaults) );
    fprintf(fptr, "Total blocked state duration = %llu \n", resultStats.totalBlockedStateDuration );
    fprintf(fptr, "Total amount of time spent in OS mode = %llu \n", resultStats.OSModetime );
    fprintf(fptr, "Total amount of time spent in user mode = %llu \n", resultStats.userModeTime );
    fprintStats(resultStats.perProcessStats, fptr);
    
    fclose(fptr);
    return 1;
}

/* Reads next line of the trace file and returns a process control block for it. 
 * Initializes the name, start_time, cpu_time and memoryFilename in the PCB
 */
struct PCB* readNextTrace(FILE *fptr){
    char* line = NULL;
    ssize_t read;
    size_t len = 0;
    struct PCB* p = NULL;
    char *token;
    char* extension = ".txt";

    if((read = getline(&line, &len, fptr)) != -1)
    {
        p = malloc(sizeof(PCBNode));

        token = strtok(line, " ");
        if((strcmp(token, "")==0) || (strcmp(token, "\n")==0) || (strcmp(token, " ")==0))
        {
            printf("Line in tracefile contains no data.\n");
            return NULL;
        }
        p->name = token;

        char* filename = malloc(sizeof(token) + sizeof(extension) + 1 ); /* make space for the new string (should check the return value ...) */
        strcpy(filename, token); /* copy name into the new var */
        strcat(filename, extension); /* add the extension */
        p->memoryFilename = filename;

        token = strtok(NULL, " ");
        p->start_time = atoi(token);
        p->memReq = gll_init();
        p->hitCount = 0;
        p->missCount = 0;
        p->fracLeft = 0;
        p->user_time = 0;
        p->OS_time = 0;
    }
   return p;
}

/* Reads the next instruction in processx.txt.
 * Returns the address/type of the next MEM/NONMEM access.*/
struct NextMem* readNextMem(FILE* fptr)
{
    char* line = NULL;
    ssize_t read;
    size_t len = 0;

    if(fptr == NULL)
    {
        return NULL;
    }

    if((read = getline(&line, &len, fptr)) != -1)
    {
        struct NextMem* lineRead = (struct NextMem*)malloc(sizeof(struct NextMem));
        line[strcspn(line, "\n")] = '\0'; //removes trailing newline characters, if any

        if (strcmp(line, "NONMEM") == 0) {
            lineRead->type = "NONMEM";
            lineRead->address = NULL;
        }
        else {
            lineRead->type = "MEM";
            lineRead->address = (char*) malloc(sizeof(strlen(line)-4));
            strcpy(lineRead->address, line+4);
        }
        return lineRead;
    }
    return NULL;
}

int readNumIns(FILE* fptr)
{
    char* line = NULL;
    ssize_t read;
    size_t len = 0;
    char *token;

    if(fptr == NULL)
    {
        return -1;
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        return atoi(token);
    }
    
    printf("Error reading system parameter from input file\n");
    exit(1);
}

/* Reads the system parameters from the input file. (from non_mem_inst_length to Num_procs)
 * Returns a struct containing the values.
 */
struct SystemParameters* readSysParam(FILE* fptr)
{
    //Check if the input is valid
    if(fptr == NULL)
    {
        printf("No file found to read input.\n");
        exit(1);
    }

    struct SystemParameters* sysParam = malloc(sizeof(systemParameters));
    char* line = NULL;
    char* TLBtypeLine = NULL;
    char* TLBrepLine = NULL;
    char* PGrepLine = NULL;
    ssize_t read;
    size_t len = 0;
    char *token;
    
    //reading comment lines
    read = getline(&line, &len, fptr);
    read = getline(&line, &len, fptr);

    //start reading
    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->non_mem_inst_length = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->virtual_addr_size_in_bits = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->DRAM_size_in_MB = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->TLB_size_in_entries = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->TLB_latency = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->DRAM_latency = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }


    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->Swap_latency = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->Page_fault_trap_handling_time = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->Swap_interrupt_handling_time = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }
    
    read = getline(&TLBtypeLine, &len, fptr);
    if(read != -1)
    {
        token = strtok(TLBtypeLine, " ");
        token = strtok(NULL, " ");
        token[strcspn(token, "\n")] = '\0'; //removes trailing newline characters, if any
        sysParam->TLB_type = token;
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&TLBrepLine, &len, fptr);
    if(read != -1)
    {
        token = strtok(TLBrepLine, " ");
        token = strtok(NULL, " ");
        token[strcspn(token, "\n")] = '\0'; //removes trailing newline characters, if any
        sysParam->TLB_replacement_policy = token;
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    //reading comment line
    read = getline(&line, &len, fptr);

    read = getline(&line, &len, fptr);

    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->P_in_bits = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }


    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->Frac_mem_inst = atof(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }
    
    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->Num_pagetable_levels = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->N1_in_bits = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }


    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->N2_in_bits = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->N3_in_bits = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }


    read = getline(&PGrepLine, &len, fptr);
    if(read != -1)
    {
        token = strtok(PGrepLine, " ");
        token = strtok(NULL, " ");
        token[strcspn(token, "\n")] = '\0'; //removes trailing newline characters, if any
        sysParam->Page_replacement_policy = token;
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }


    read = getline(&line, &len, fptr);
    if(read != -1)
    {
        token = strtok(line, " ");
        token = strtok(NULL, " ");
        sysParam->Num_procs = atoi(token);
    }
    else{
        printf("Error reading system parameter from input file\n");
        exit(1);
    }

    //reading comment line
    read = getline(&line, &len, fptr);

    return sysParam;
}