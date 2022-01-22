#ifndef DATASTRUCTURES_H 
#define DATASTRUCTURES_H

#include <stdio.h>
#include <stdlib.h>
#include "gll.h"

typedef struct PCB ProcessControlBlock;

struct PCB
{
    char* name; //the process name (of the form processx)
    uint64_t start_time;
    char* memoryFilename; //the process trace file name (of the form processx.txt)
    FILE* memoryFile;
    gll_t* memReq; //a list of all instructions in processx.txt
    int numOfIns;
    int hitCount; //for page table
    int missCount; //for page table
    uint64_t fracLeft; //the time left to execute the current instruction
    uint64_t blockOccur;
    void **topLevelPageTable; //top-level page table of the process
    int numberOfTLBhit; //number of TLB hit
    int numberOfTLBmiss; //number of TLB miss
    uint64_t blockedStateDuration;
    uint64_t numberOfContextSwitch;
    uint64_t numberOfDiskInt;

    uint64_t OS_time;
    uint64_t user_time;
} PCBNode;

//An instruction
struct NextMem
{
    char* type;
    char* address;
};

//Stats of a processes
typedef struct Stats
{
    char* processName;
    int hitCount;
    int missCount;
    uint64_t duration;
    int numberOfTLBhit;
    int numberOfTLBmiss;
    uint64_t blockedStateDuration;
    uint64_t numberOfContextSwitch;
    uint64_t numberOfDiskInt;

    uint64_t OS_time;
    uint64_t user_time;
} stats;

//Stats of all processes within a given inputx.txt file
typedef struct TotalStats
{
    uint64_t start_time;
    uint64_t end_time;
    gll_t* perProcessStats;
    int numberOfContextSwitch;
    int numberOfDiskInt;
    int totalPgHits;
    int totalPgFaults;
    int totalTLBhit;
    int totalTLBmiss;
    uint64_t totalBlockedStateDuration;
    uint64_t OSModetime;
    uint64_t userModeTime;
    gll_t* executionOrder; //the execution order of the processes
} totalstats;

typedef struct SystemParameters
{
    uint64_t non_mem_inst_length;
    int virtual_addr_size_in_bits;
    uint64_t contextSwitchTime;

    uint64_t TLB_latency;
    uint64_t DRAM_latency;
    uint64_t Swap_latency;
    uint64_t Page_fault_trap_handling_time;
    uint64_t Swap_interrupt_handling_time; 

    uint64_t quantum;

    int DRAM_size_in_MB;
    int TLB_size_in_entries; 
    int P_in_bits;

    char* TLB_replacement_policy;
    char* TLB_type;

    double Frac_mem_inst;
    int Num_pagetable_levels;
    int N1_in_bits;
    int N2_in_bits;
    int N3_in_bits;
    char* Page_replacement_policy;
    int Num_procs;
} systemParameters;

//A TLB entry
typedef struct
{
    uint32_t valid;
    uint64_t time;
    uint32_t vpn;
    uint32_t ppn;
} TLBentry;

//Information of a page in DRAM
typedef struct
{
    uint64_t time; //timestamp for LRU, reference bit for clock
    struct PCB *process;
    uint32_t vpn;
} DRAM_page;

//A page table entry
typedef struct
{
    uint32_t valid;
    uint32_t ppn;
} pte;

#endif
