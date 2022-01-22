#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "fileIO.h"
#include "dataStructures.h"

int debug = 0; //flag indicates if an error occurs

char* traceFileName; //of the form inputx.txt
FILE* traceptr; //input file object corresponding to inputx.txt

char* outputFileName; //of the form ouputx.txt

struct SystemParameters* sysParam;

gll_t *processList, //all processes that are not ready
      *readyProcess, //ready processes
      *runningProcess, //running processes
      *blockedProcess; //blocked processes

struct TotalStats resultStats; //the result stats of all processes

uint64_t current_time, //Current time
         nextQuanta, //Next time interrupt
         nextDiskInt; //Next disk interrupt

uint64_t OSTime;
uint64_t userTime;

int numberContextSwitch;

int numberDiskInt;

uint64_t contextSwitchTime = 1000;

uint64_t quantum = 10000;

struct PCB *previousProcess; //The previously running process

uint64_t lastStop; //The finish time of the last disk to memory transfer

uint64_t totalTLBhit; //total number of TLB hit

uint64_t totalTLBmiss; //total number of TLB miss

uint64_t totalPgHits; //total number of page accesses that do not raise a page fault trap

uint64_t totalPgFaults; //total number of page faults

uint64_t totalBlockedDuration; //total fraction of time in blocked state

int page_replacement_policy; //LRU(0) or clock(1)
uint16_t clockHand; //The clock hand for the clock algorithm

TLBentry *TLB; //TLB
DRAM_page *DRAM; //DRAM

/*Helper print functions*/
void printPCB(void* p);
void printStats(void* p);
/* Prints the order of execution of the processes */
void printExecOrder(void* c);

void init();
void finishAll();

/*Simulates the process simulator mechanism*/
void simulate();
/*Simulates the running of one process until it has to stop. Stops when there is a timer interrupt(1), disk interrupt(4), page fault(0), process finishes(-1) and if no running process to run(2). returns why it stopped (return values in braces)*/
int processSimulator();
/*Reads one instruction/address from the trace file. Has to check TLB/page table. Has to call page replacement or report page fault(-1). returns 1 on success, 0 if trace ends, -1 if page fault*/
int readPage(struct PCB* p, uint64_t stopTime);
/* Moves page from disk to memory after a disk interrupt occurs. Should take care of multiple if happens at the same time */
void diskToMemory();
/* Schedules next ready process to run in a round robin fashion */
void schedulingRR(int pauseCause);
/* Process done. Update statistics and close files. */
void cleanUpProcess(struct PCB* p);

/*Statistics related functions*/
void statsinit(); //Initialize some result stats
void statsUpdate();

//helper functions
uint32_t getVPN( char *vaddress );

void TLBflush(); //Flush the TLB

/*
 * Find the required PTE according to a given VPN
 * Return -1 if page falt occurs. Otherwise, return the corresponding PPN
 */
int findPTE( struct PCB *p, uint32_t vpn );

/*
 * Add a translation into TLB. If TLB is full, perform TLB replacement.
 */
void TLBadd( uint32_t vpn, uint32_t ppn );

void TLBdelete( uint32_t vpn );

uint32_t DRAMadd( struct PCB *p, uint32_t vpn );

void addPTE( struct PCB *p, uint32_t vpn, uint32_t ppn );

void deletePTE( struct PCB *p, uint32_t vpn );
