#include "simulator.h"
#include <string.h>

void init()
{
    //Initiallization
    current_time = 0;
    nextQuanta = current_time + quantum;
    readyProcess = gll_init();
    runningProcess= gll_init();
    blockedProcess = gll_init();
    processList = gll_init();
    traceptr = openTrace(traceFileName); //open inputx.txt
    sysParam = readSysParam(traceptr); //read the system parameters

    //read all processes from inputx.txt and put them in the processList
    struct PCB* temp = readNextTrace(traceptr);
    if(temp == NULL)
    {
        printf("No data in file. Exit.\n");
        exit(1);
    }
    while(temp != NULL)
    {
        //Initialization
        temp->topLevelPageTable = NULL;
        temp->numberOfTLBhit = 0;
        temp->numberOfTLBmiss = 0;
        temp->blockedStateDuration = 0;
        temp->numberOfContextSwitch = 0;
        temp->numberOfDiskInt = 0;

        gll_pushBack(processList, temp);
        temp = readNextTrace(traceptr);
    }

    //transfer arrived processes from processList to readyProcess list
    temp = gll_first(processList);
    
    while((temp!= NULL) && ( temp->start_time <= current_time))
    {
        struct NextMem* tempAddr;
        temp->memoryFile = openTrace(temp->memoryFilename);
        temp->numOfIns = readNumIns(temp->memoryFile);
        
        //Get all instructions in the processx.txt
        tempAddr = readNextMem(temp->memoryFile);
        while(tempAddr!= NULL)
        {
            gll_pushBack(temp->memReq, tempAddr);
            tempAddr = readNextMem(temp->memoryFile);
        }

        gll_pushBack(readyProcess, temp);
        gll_pop(processList);

        temp = gll_first(processList);
    }

    //TODO: Initialize what you need
    OSTime = 0;
    userTime = 0;
    numberContextSwitch = 0;
    numberDiskInt = 0;
    lastStop = 0;
    totalTLBhit = 0;
    totalTLBmiss = 0;
    totalPgHits = 0;
    totalPgFaults = 0;
    totalBlockedDuration = 0;

    //Initialize TLB
    TLB = (TLBentry *)malloc( sysParam->TLB_size_in_entries * sizeof(TLBentry) );
    TLBflush();

    //Initialize DRAM
    uint32_t num_DRAM_page = 1;
    num_DRAM_page = num_DRAM_page << ( 20 - sysParam->P_in_bits );
    num_DRAM_page *= sysParam->DRAM_size_in_MB;
    DRAM = (DRAM_page *)malloc( num_DRAM_page * sizeof(DRAM_page) );
    uint32_t i = 0;
    while ( i < num_DRAM_page )
    {
        DRAM[i].time = 0;
        DRAM[i].process = NULL;
        i++;
    }
}

void finishAll()
{
    if((gll_first(readyProcess)!= NULL) || (gll_first(runningProcess)!= NULL) || (gll_first(blockedProcess)!= NULL) || (gll_first(processList)!= NULL))
    {
        printf("Something is still pending\n");
    }
    gll_destroy(readyProcess);
    gll_destroy(runningProcess);
    gll_destroy(blockedProcess);
    gll_destroy(processList);

    //TODO: Anything else you want to destroy
    free( DRAM );
    free( TLB );
    
    closeTrace(traceptr);
}

void statsinit()
{
    resultStats.perProcessStats = gll_init();
    resultStats.executionOrder = gll_init();
    resultStats.start_time = current_time;
}

void statsUpdate()
{
    resultStats.OSModetime = OSTime;
    resultStats.userModeTime  = userTime;   
    resultStats.numberOfContextSwitch = numberContextSwitch;
    resultStats.end_time = current_time;
    resultStats.totalTLBhit = totalTLBhit;
    resultStats.totalTLBmiss = totalTLBmiss;
    resultStats.totalPgHits = totalPgHits;
    resultStats.totalPgFaults = totalPgFaults;
    resultStats.totalBlockedStateDuration = totalBlockedDuration;
    resultStats.numberOfDiskInt = numberDiskInt;
}

/* Try executing one instruction. Check TLB/page table. If page fault occurs, make page replacement.
 * returns 1 on success, 0 if trace ends, -1 if page fault
 */
int readPage(struct PCB* p, uint64_t stopTime)
{
    struct NextMem* addr = gll_first(p->memReq); //Get next instruction
    uint64_t timeAvailable = stopTime - current_time;
    
    if(addr == NULL)
    {
        return 0;
    }
    if(debug == 1)
    {
        printf("Request::%s::%s::\n", addr->type, addr->address);
    }

    if(strcmp(addr->type, "NONMEM") == 0)
    {
        uint64_t timeNeeded = (p->fracLeft > 0)? p->fracLeft: sysParam->non_mem_inst_length;
    
        if(timeAvailable < timeNeeded)
        {
            current_time += timeAvailable;
            userTime += timeAvailable;
            p->user_time += timeAvailable;
            p->fracLeft = timeNeeded - timeAvailable;
        }
        else{
            gll_pop(p->memReq);
            current_time += timeNeeded; 
            userTime += timeNeeded;
            p->user_time += timeNeeded;
            p->fracLeft = 0;
        }

        if(gll_first(p->memReq) == NULL)
        {
            return 0;
        }
        return 1;
    }
    else
    {
        //TODO: for MEM traces
        uint32_t vpn = getVPN( addr->address );
        uint32_t ppn;

        //Check if TLB has the translation
        p->user_time += sysParam->TLB_latency;
        userTime += sysParam->TLB_latency;
        current_time += sysParam->TLB_latency;
        timeAvailable -= sysParam->TLB_latency;

        int isInTLB = 0;
        int i = 0;
        while ( i < sysParam->TLB_size_in_entries )
        {
            if ( TLB[i].valid == 1 && TLB[i].vpn == vpn )
            {
                isInTLB = 1;
                TLB[i].time = current_time;
                break;
            }
            i++;
        }

        if ( isInTLB == 0 ) //If TLB miss, 
        {
            p->numberOfTLBmiss++;
            totalTLBmiss++;
            if ( timeAvailable >= sysParam->DRAM_latency ) //If time allows, look up page table in DRAM
            {
                p->user_time += sysParam->DRAM_latency;
                userTime += sysParam->DRAM_latency;
                current_time += sysParam->DRAM_latency;
                timeAvailable -= sysParam->DRAM_latency;
                int ppn = findPTE( p, vpn );
                if ( ppn == -1 ) //If page fault occurs
                {
                    totalPgFaults++;
                    p->missCount++;
                    current_time += sysParam->Page_fault_trap_handling_time;
                    p->OS_time += sysParam->Page_fault_trap_handling_time;
                    OSTime += sysParam->Page_fault_trap_handling_time;
                    p->fracLeft = sysParam->Swap_latency;
                    p->blockOccur = current_time;
                    return -1;
                }
                else
                {
                    totalPgHits++;
                    p->hitCount++;
                    TLBadd( vpn, ppn );

                    //If time allows, access the corresponding page in the main memory
                    if ( timeAvailable >= sysParam->DRAM_latency )
                    {
                        userTime += sysParam->DRAM_latency;
                        p->user_time += sysParam->DRAM_latency;
                        current_time += sysParam->DRAM_latency;

                        if ( page_replacement_policy == 0 )
                            DRAM[ ppn ].time = current_time;
                        else
                            DRAM[ ppn ].time = 1;

                        gll_pop( p->memReq );
                    }
                    else
                    {
                        userTime += stopTime - current_time;
                        p->user_time += stopTime - current_time;
                        current_time = stopTime;
                        if ( stopTime == nextDiskInt ) //If the process encounters a disk interrupt, increase numberOfDiskInt
                            p->numberOfContextSwitch++;
                    }
                }
            }
            else
            {
                userTime += stopTime - current_time;
                p->user_time += stopTime - current_time;
                current_time = stopTime;
                if ( stopTime == nextDiskInt ) //If the process encounters a disk interrupt, increase numberOfDiskInt
                    p->numberOfDiskInt++;
            }
        }
        else //If TLB hits, 
        {
            p->numberOfTLBhit++;
            totalTLBhit++;
            //If time allows, access the corresponding page in the main memory
            if ( timeAvailable >= sysParam->DRAM_latency )
            {
                userTime += sysParam->DRAM_latency;
                p->user_time += sysParam->DRAM_latency;
                current_time += sysParam->DRAM_latency;

                if ( page_replacement_policy == 0 )
                    DRAM[ TLB[i].ppn ].time = current_time;
                else
                    DRAM[ TLB[i].ppn ].time = 1;

                gll_pop( p->memReq );
            }
            else
            {
                userTime += stopTime - current_time;
                p->user_time += stopTime - current_time;
                current_time = stopTime;
                if ( stopTime == nextDiskInt ) //If the process encounters a disk interrupt, increase numberOfDiskInt
                    p->numberOfDiskInt++;
            }
        }
    }
    return 1;
}

void schedulingRR(int pauseCause)
{
    //move first readyProcess to running
    gll_push(runningProcess, gll_first(readyProcess));
    gll_pop(readyProcess);

    if(gll_first(runningProcess) != NULL)
    {
        current_time = current_time + contextSwitchTime;
        OSTime += contextSwitchTime;
        struct PCB* temp = gll_first(runningProcess);
        if ( strcmp( gll_last( resultStats.executionOrder ), temp->name ) != 0 )
        {
            if ( previousProcess )
            {
                previousProcess->numberOfContextSwitch++;
            }
            else
            {
                struct Stats *temp2 = gll_last(resultStats.perProcessStats);
                temp2->numberOfContextSwitch++;
            }
            numberContextSwitch++;
            previousProcess = gll_first(runningProcess);
            TLBflush();
            gll_pushBack(resultStats.executionOrder, temp->name);
        }
    }
}

/*runs a process in running state. returns 0 if page fault, 1 if quanta finishes, -1 if traceFile ends, 2 if no running process, 4 if disk Interrupt*/
int processSimulator()
{
    uint64_t stopTime = nextQuanta; //the time when the process need to stop
    int stopCondition = 1;
    if(gll_first(runningProcess)!=NULL)
    {
        //TODO
        //if(TODO: if there is a pending disk operation in the future)
        //{
            //TODO: stopTime = occurance of the first disk interrupt
        //    stopCondition = 4;
        //}
        if ( gll_first(blockedProcess) )
        {
            struct PCB *temp = gll_first(blockedProcess);
            nextDiskInt = current_time + temp->fracLeft;
            if ( nextQuanta > nextDiskInt )
            {
                stopTime = nextDiskInt;
                stopCondition = 4;
            }
        }

        //run the process until stopTime
        while(current_time < stopTime)
        {
            int read = readPage(gll_first(runningProcess), stopTime); //Try executing one instruction
            if(debug == 1){
                printf("Read: %d\n", read);
                printf("Current Time %" PRIu64 ", Next Quanta Time %" PRIu64 " %" PRIu64 "\n",current_time, nextQuanta, stopTime);
            }
            if(read == 0)
            {
                return -1;
                break;
            }
            else if(read == -1) //page fault
            {
                if(gll_first(runningProcess) != NULL)
                {

                    gll_pushBack(blockedProcess, gll_first(runningProcess));
                    gll_pop(runningProcess);
                    return 0;
                }
            }
        }
        if(debug == 1)
        {
            printf("Stop condition found\n");
            printf("Current Time %" PRIu64 ", Next Quanta Time %" PRIu64 "\n",current_time, nextQuanta);
        }
        return stopCondition;
    }
    if(debug == 1)
    {
        printf("No running process found\n");
    }
    return 2;
}

void cleanUpProcess(struct PCB* p)
{
    struct PCB* temp = p;

    //TODO: Adjust the amount of available memory as this process is finishing
    //Free the page table
    uint32_t num_top_entries = 1;
    num_top_entries = num_top_entries << sysParam->N1_in_bits;

    uint32_t num_second_entries = 1;
    if ( sysParam->Num_pagetable_levels > 1 )
        num_second_entries = num_second_entries << sysParam->N2_in_bits;
    else
        num_second_entries = 0;

    uint32_t num_third_entries = 1;
    if ( sysParam->Num_pagetable_levels > 2 )
        num_third_entries = num_third_entries << sysParam->N3_in_bits;
    else
        num_third_entries = 0;


    void **topLevelPageTable = p->topLevelPageTable;
    if ( topLevelPageTable )
    {
        uint32_t i = 0;
        while ( i < num_top_entries )
        {
            if ( topLevelPageTable[i] == NULL )
            {
                i++;
                continue;
            }
            void **secondLevelPageTable = (void **)( topLevelPageTable[i] );

            uint32_t j = 0;
            while ( j < num_second_entries )
            {
                if ( secondLevelPageTable[j] == NULL )
                {
                    j++;
                    continue;
                }
                pte **thirdLevelPageTable = (pte **)( secondLevelPageTable[j] );

                uint32_t k = 0;
                while ( k <num_third_entries )
                {
                    if ( thirdLevelPageTable[k] == NULL )
                    {
                        k++;
                        continue;
                    }
                    free( thirdLevelPageTable[k] );
                    k++;
                }
                free( secondLevelPageTable[j] );
                j++;
            }
            free( topLevelPageTable[i] );
            i++;
        }
        free( topLevelPageTable );
        p->topLevelPageTable = NULL;
    }

    struct Stats* s = malloc(sizeof(stats));
    s->processName = temp->name;
    s->hitCount = temp->hitCount;
    s->missCount = temp->missCount;
    s->user_time = temp->user_time;
    s->OS_time = temp->OS_time;
    s->numberOfTLBhit = temp->numberOfTLBhit;
    s->numberOfTLBmiss = temp->numberOfTLBmiss;
    s->duration = current_time - temp->start_time;
    s->blockedStateDuration = temp->blockedStateDuration;
    s->numberOfDiskInt = temp->numberOfDiskInt;
    s->numberOfContextSwitch = temp->numberOfContextSwitch;

    gll_pushBack(resultStats.perProcessStats, s);
    
    gll_destroy(temp->memReq);
    closeTrace(temp->memoryFile);
}

uint32_t getVPN( char *vaddress )
{
    char temp[8];
    memcpy( temp, vaddress + 2, 8 ); //skip "0x"
    uint32_t vpn = (uint32_t)strtol(temp, NULL, 16); //convert to uint32_t
    vpn = vpn >> sysParam->P_in_bits; //eliminate VPO
    return vpn;
}

void TLBflush()
{
    uint64_t i = 0;
    while ( i < sysParam->TLB_size_in_entries )
    {
        TLB[i].valid = 0;
        TLB[i].time = 0;
        TLB[i].vpn = -1;
        TLB[i].ppn = -1;
        i++;
    }
}

int findPTE( struct PCB *p, uint32_t vpn )
{
    if ( p == NULL )
        return -1;

    void **topLevelPageTable = p->topLevelPageTable;
    if ( topLevelPageTable == NULL )
        return -1;

    uint32_t firstLvlIndex = vpn >> (sysParam->N2_in_bits + sysParam->N3_in_bits);
    uint32_t secondLvlIndex = ( vpn - ( firstLvlIndex << (sysParam->N2_in_bits + sysParam->N3_in_bits) ) )
                              >> sysParam->N3_in_bits;
    uint32_t thirdLvlIndex = vpn - ( firstLvlIndex << (sysParam->N2_in_bits + sysParam->N3_in_bits) )
                                 - ( secondLvlIndex << sysParam->N3_in_bits );
    if ( sysParam->Num_pagetable_levels == 3 )
    {
        void **secondLevelPageTable = (void **)( *(topLevelPageTable + firstLvlIndex) );
        if ( secondLevelPageTable == NULL )
            return -1;
        else
        {
            pte **thirdLevelPageTable = (pte **)( *(secondLevelPageTable + secondLvlIndex) );
            if ( thirdLevelPageTable == NULL )
                return -1;
            else
            {
                pte *temp_pte = *(thirdLevelPageTable + thirdLvlIndex);
                if ( temp_pte == NULL || temp_pte->valid == 0 )
                    return -1;
                else
                {
                    return (int)temp_pte->ppn;
                }
            }
        }
    }
    else if ( sysParam->Num_pagetable_levels == 2 )
    {
        pte **secondLevelPageTable = (pte **)( *(topLevelPageTable + firstLvlIndex) );
        if ( secondLevelPageTable == NULL )
            return -1;
        else
        {
            pte *temp_pte = (pte *) *(secondLevelPageTable + secondLvlIndex);
            if ( temp_pte == NULL || temp_pte->valid == 0 )
                return -1;
            else
            {
                return (int)temp_pte->ppn;
            }
        }
    }
    else
    {
        pte *temp_pte = (pte *) *(topLevelPageTable + firstLvlIndex);
        if ( temp_pte == NULL || temp_pte->valid == 0 )
            return -1;
        else
        {
            return (int)temp_pte->ppn;
        }
    }
}

void TLBadd( uint32_t vpn, uint32_t ppn )
{
    int replace = -1;
    uint64_t minTime = current_time + 1;

    uint64_t i = 0;
    while ( i < sysParam->TLB_size_in_entries )
    {
        if ( TLB[i].valid == 0 )
        {
            TLB[i].valid = 1;
            TLB[i].time = current_time;
            TLB[i].vpn = vpn;
            TLB[i].ppn = ppn;
            return;
        }

        if ( TLB[i].time < minTime )
        {
            minTime = TLB[i].time;
            replace = i;
        }
        i++;
    }

    TLB[replace].time = current_time;
    TLB[replace].vpn = vpn;
    TLB[replace].ppn = ppn;
    return;
}

void TLBdelete( uint32_t vpn )
{
    uint64_t i = 0;
    while ( i < sysParam->TLB_size_in_entries )
    {
        if ( TLB[i].vpn == vpn )
        {
            TLB[i].valid = 0;
            TLB[i].time = 0;
            TLB[i].vpn = -1;
            TLB[i].ppn = -1;
        }
        i++;
    }
    return;
}

uint32_t DRAMadd( struct PCB *p, uint32_t vpn )
{
    uint32_t num_DRAM_page = 1;
    num_DRAM_page = num_DRAM_page << ( 20 - sysParam->P_in_bits );
    num_DRAM_page *= sysParam->DRAM_size_in_MB;

    if ( page_replacement_policy == 0 ) //If the page replacement policy is LRU
    {
        uint64_t minTime = current_time + 1;
        int replace = -1;

        uint32_t i = 0;
        while ( i < num_DRAM_page )
        {
            if ( DRAM[i].time == 0 )
            {
                DRAM[i].time = current_time;
                DRAM[i].process = p;
                DRAM[i].vpn = vpn;
                return i;
            }

            if ( DRAM[i].time < minTime )
            {
                minTime = DRAM[i].time;
                replace = i;
            }
            i++;
        }
        deletePTE( DRAM[replace].process, DRAM[replace].vpn );
        TLBdelete( DRAM[replace].vpn );

        DRAM[replace].time = current_time;
        DRAM[replace].process = p;
        DRAM[replace].vpn = vpn;

        return (uint32_t)replace;
    }
    else //If using clock algorithm
    {
        while( 1 )
        {
            if ( DRAM[clockHand].time == 0 )
            {
                if ( DRAM[clockHand].process != NULL )
                {
                    deletePTE( DRAM[clockHand].process, DRAM[clockHand].vpn );
                    TLBdelete( DRAM[clockHand].vpn );
                }
                DRAM[clockHand].time = 1;
                DRAM[clockHand].process = p;
                DRAM[clockHand].vpn = vpn;
                return clockHand;
            }
            else
                DRAM[clockHand].time--;

            if ( clockHand + 1 < num_DRAM_page )
                clockHand++;
            else
                clockHand = 0;
        }
    }
}

void addPTE( struct PCB *p, uint32_t vpn, uint32_t ppn )
{
    pte *temp_pte;
    uint32_t firstLvlIndex = vpn >> (sysParam->N2_in_bits + sysParam->N3_in_bits);
    uint32_t secondLvlIndex = ( vpn - ( firstLvlIndex << (sysParam->N2_in_bits + sysParam->N3_in_bits) ) )
                              >> sysParam->N3_in_bits;
    uint32_t thirdLvlIndex = vpn - ( firstLvlIndex << (sysParam->N2_in_bits + sysParam->N3_in_bits) )
                                 - ( secondLvlIndex << sysParam->N3_in_bits );
    
    if ( sysParam->Num_pagetable_levels == 3 )
    {
        void **topLevelPageTable = p->topLevelPageTable;
        if ( topLevelPageTable == NULL )
        {
            uint32_t num_top_entries = 1;
            num_top_entries = num_top_entries << sysParam->N1_in_bits;
            topLevelPageTable = malloc( num_top_entries * sizeof(void *) );

            uint32_t i = 0;
            while ( i < num_top_entries )
            {
                topLevelPageTable[i] = NULL;
                i++;
            }
            p->topLevelPageTable = topLevelPageTable;
        }

        void **secondLevelPageTable = (void **)( topLevelPageTable[ firstLvlIndex ] );
        if ( secondLevelPageTable == NULL )
        {
            uint32_t num_second_entries = 1;
            num_second_entries = num_second_entries << sysParam->N2_in_bits;
            secondLevelPageTable = malloc( num_second_entries * sizeof(void *) );

            uint32_t i = 0;
            while ( i < num_second_entries )
            {
                secondLevelPageTable[i] = NULL;
                i++;
            }
            topLevelPageTable[ firstLvlIndex ] = (void *)secondLevelPageTable;
        }

        pte **thirdLevelPageTable = (pte **)( secondLevelPageTable[ secondLvlIndex ] );
        if ( thirdLevelPageTable == NULL )
        {
            uint32_t num_third_entries = 1;
            num_third_entries = num_third_entries << sysParam->N3_in_bits;
            thirdLevelPageTable = malloc( num_third_entries * sizeof(pte *) );

            uint32_t i = 0;
            while ( i < num_third_entries )
            {
                thirdLevelPageTable[i] = NULL;
                i++;
            }
            secondLevelPageTable[ secondLvlIndex ] = (void *)thirdLevelPageTable;
        }

        if ( thirdLevelPageTable[ thirdLvlIndex ] == NULL )
            thirdLevelPageTable[ thirdLvlIndex ] = malloc( sizeof(pte) );

        temp_pte = thirdLevelPageTable[ thirdLvlIndex ];
    }
    else if ( sysParam->Num_pagetable_levels == 2 )
    {
        void **topLevelPageTable = p->topLevelPageTable;
        if ( topLevelPageTable == NULL )
        {
            uint32_t num_top_entries = 1;
            num_top_entries = num_top_entries << sysParam->N1_in_bits;
            topLevelPageTable = malloc( num_top_entries * sizeof(void *) );

            uint32_t i = 0;
            while ( i < num_top_entries )
            {
                topLevelPageTable[i] = NULL;
                i++;
            }
            p->topLevelPageTable = topLevelPageTable;
        }

        pte **secondLevelPageTable = (pte **)( topLevelPageTable[ firstLvlIndex ] );
        if ( secondLevelPageTable == NULL )
        {
            uint32_t num_second_entries = 1;
            num_second_entries = num_second_entries << sysParam->N2_in_bits;
            secondLevelPageTable = malloc( num_second_entries * sizeof(pte *) );

            uint32_t i = 0;
            while ( i < num_second_entries )
            {
                secondLevelPageTable[i] = NULL;
                i++;
            }
            topLevelPageTable[ firstLvlIndex ] = (void *)secondLevelPageTable;
        }

        if ( secondLevelPageTable[ secondLvlIndex ] == NULL )
            secondLevelPageTable[ secondLvlIndex ] = malloc( sizeof(pte) );

        temp_pte = secondLevelPageTable[ secondLvlIndex ];
    }
    else
    {
        pte **topLevelPageTable = (pte **)(p->topLevelPageTable);
        if ( topLevelPageTable == NULL )
        {
            uint32_t num_top_entries = 1;
            num_top_entries = num_top_entries << sysParam->N1_in_bits;
            topLevelPageTable = malloc( num_top_entries * sizeof(pte *) );

            uint32_t i = 0;
            while ( i < num_top_entries )
            {
                topLevelPageTable[i] = NULL;
                i++;
            }
            p->topLevelPageTable = (void **)topLevelPageTable;
        }

        if ( topLevelPageTable[ firstLvlIndex ] == NULL )
            topLevelPageTable[ firstLvlIndex ] = malloc( sizeof(pte) );

        temp_pte = topLevelPageTable[ firstLvlIndex ];
    }
    
    temp_pte->ppn = ppn;
    temp_pte->valid = 1;
    return;
}

int printPageTables( void **topLevelPageTable )
{
    uint32_t num_top_entries = 1;
    num_top_entries = num_top_entries << sysParam->N1_in_bits;

    uint32_t num_second_entries = 1;
    if ( sysParam->Num_pagetable_levels > 1 )
        num_second_entries = num_second_entries << sysParam->N2_in_bits;
    else
        num_second_entries = 0;

    uint32_t num_third_entries = 1;
    if ( sysParam->Num_pagetable_levels > 2 )
        num_third_entries = num_third_entries << sysParam->N3_in_bits;
    else
        num_third_entries = 0;

    uint32_t i = 0;
    while ( i < num_top_entries )
    {
        if ( topLevelPageTable[i] != NULL )
        {
            printf("Top Level Page Table entry: %p\n", &topLevelPageTable[i] );
            void **secondLevelPageTable = (void **)topLevelPageTable[i];
            uint32_t j = 0;
            while ( j < num_second_entries )
            {
                if ( secondLevelPageTable[j] != NULL )
                {
                    printf("  Second Level Page Table entry: %p\n", &secondLevelPageTable[j] );
                    pte **thirdLevelPageTable = (pte **)secondLevelPageTable[j];
                    uint32_t k = 0;
                    while ( k < num_third_entries )
                    {
                        if ( thirdLevelPageTable[k] != NULL )
                        {
                            printf("    Third Level Page Table entry: %p, corresponding ppn: %d\n", &thirdLevelPageTable[k], thirdLevelPageTable[k]->ppn );
                        }
                        k++;
                    }
                }
                j++;
            }
            printf("\n");
        }
        i++;
    }
    return 0;
}

void deletePTE( struct PCB *p, uint32_t vpn )
{
    if ( p == NULL )
        return;

    uint32_t firstLvlIndex = vpn >> (sysParam->N2_in_bits + sysParam->N3_in_bits);
    uint32_t secondLvlIndex = ( vpn - ( firstLvlIndex << (sysParam->N2_in_bits + sysParam->N3_in_bits) ) )
                              >> sysParam->N3_in_bits;
    uint32_t thirdLvlIndex = vpn - ( firstLvlIndex << (sysParam->N2_in_bits + sysParam->N3_in_bits) )
                                 - ( secondLvlIndex << sysParam->N3_in_bits );

    void **topLevelPageTable = p->topLevelPageTable;
    void **secondLevelPageTable;
    pte **thirdLevelPageTable;
    if ( topLevelPageTable == NULL )
        return;

    if ( sysParam->Num_pagetable_levels == 3 )
    {
        secondLevelPageTable = (void **)( *(topLevelPageTable + firstLvlIndex) );
        if ( secondLevelPageTable == NULL )
            return;
        else
        {
            thirdLevelPageTable = (pte **)( *(secondLevelPageTable + secondLvlIndex) );
            if ( thirdLevelPageTable == NULL )
                return;
            else
            {
                pte *temp_pte = *(thirdLevelPageTable + thirdLvlIndex);
                if ( temp_pte == NULL || temp_pte->valid == 0 )
                    return;
                else
                {
                    free( temp_pte );
                    *(thirdLevelPageTable + thirdLvlIndex) = NULL;
                }
            }
        }
    }
    else if ( sysParam->Num_pagetable_levels == 2 )
    {
        secondLevelPageTable = (void **)( *(topLevelPageTable + firstLvlIndex) );
        if ( secondLevelPageTable == NULL )
            return;
        else
        {
            pte *temp_pte = (pte *) *(secondLevelPageTable + secondLvlIndex);
            if ( temp_pte == NULL || temp_pte->valid == 0 )
                return;
            else
            {
                free( temp_pte );
                *(secondLevelPageTable + secondLvlIndex) = NULL;
            }
        }
    }
    else
    {
        pte *temp_pte = (pte *) *(topLevelPageTable + firstLvlIndex);
        if ( temp_pte == NULL || temp_pte->valid == 0 )
            return;
        else
        {
            free( temp_pte );
            *(topLevelPageTable + firstLvlIndex) = NULL;
        }
    }

    //Free empty page tables
    uint32_t num_DRAM_page = 1;
    num_DRAM_page = num_DRAM_page << ( 20 - sysParam->P_in_bits );
    num_DRAM_page *= sysParam->DRAM_size_in_MB;

    uint32_t num_top_entries = 1;
    num_top_entries = num_top_entries << sysParam->N1_in_bits;

    uint32_t num_second_entries = 1;
    if ( sysParam->Num_pagetable_levels > 1 )
        num_second_entries = num_second_entries << sysParam->N2_in_bits;
    else
        num_second_entries = 0;

    uint32_t num_third_entries = 1;
    if ( sysParam->Num_pagetable_levels > 2 )
        num_third_entries = num_third_entries << sysParam->N3_in_bits;
    else
        num_third_entries = 0;

    uint32_t i = 0;
    while ( i < num_third_entries )
    {
        if ( thirdLevelPageTable[i] != NULL )
        {
            pte *temp = (pte *)thirdLevelPageTable[i];
            if ( temp->ppn >= num_DRAM_page || temp->valid == 0 )
            {
                free(temp);
                thirdLevelPageTable[i] = NULL;
            }
        }
        i++;
    }

    if ( sysParam->Num_pagetable_levels > 2 )
    {
        i = 0;
        while ( i < num_third_entries )
        {
            if ( thirdLevelPageTable[i] != NULL )
                return;
            i++;
        }
        free( thirdLevelPageTable );
        secondLevelPageTable[secondLvlIndex] = NULL;
    }
    
    if ( sysParam->Num_pagetable_levels > 1 )
    {
        i = 0;
        while ( i < num_second_entries )
        {
            if ( secondLevelPageTable[i] != NULL )
                return;
            i++;
        }
        free( secondLevelPageTable );
        topLevelPageTable[firstLvlIndex] = NULL;
    }
    
}

void printPCB(void* v)
{
    struct PCB* p = v;
    if(p!=NULL){
        printf("%s, %" PRIu64 "\n", p->name, p->start_time);
    }
}

void printStats(void* v)
{
    struct Stats* s = v;
    if(s!=NULL)
    {
        printf("\n\nProcess: %s: \n", s->processName);
        printf("Completion time: %llu\n", s->duration );
        printf("Number of context switches = %d \n", s->numberOfContextSwitch );
        printf("Number of disk interrupts = %d \n", s->numberOfDiskInt );
        printf("Number of TLB misses = %d \n", s->numberOfTLBmiss );
        printf("%% of TLB misses = %lf \n", 100.0 * s->numberOfTLBmiss / (s->numberOfTLBhit + s->numberOfTLBmiss) );
        printf("Number of page faults = %d \n", s->missCount );
        printf("%% of page faults = %lf \n", 100.0 * s->missCount / (s->hitCount + s->missCount) );
        printf("Blocked state duration = %llu \n", s->blockedStateDuration );
        printf("Amount of time spent in OS mode = %llu \n", s->OS_time );
        printf("Amount of time spent in user mode = %llu \n", s->user_time );
    }
}

void printExecOrder(void* v)
{
    char* c = v;
    if(c!=NULL){
        printf("%s\n", c) ;
    }
}


void diskToMemory()
{
    // TODO: Move requests from disk to memory
    // TODO: move appropriate blocked process to ready process
    struct PCB *temp = gll_first(blockedProcess);
    if ( temp )
    {
        //Check if the page fault does occurs
        struct NextMem* nextInst = gll_first(temp->memReq);
        uint32_t vpn = getVPN( nextInst->address );
        while ( findPTE( temp, vpn ) != -1 )
        {
            temp->fracLeft = 0;
            gll_pop(blockedProcess);
            gll_pushBack(readyProcess, temp);

            temp = gll_first(blockedProcess);
            if ( temp == NULL )
                return;
            nextInst = gll_first(temp->memReq);
            vpn = getVPN( nextInst->address );
        }

        //For the next page fault do data transfer from disk to memory
        uint64_t runTime = 0;
        if ( lastStop < temp->blockOccur )
            runTime = current_time - temp->blockOccur;
        else
            runTime = current_time - lastStop;

        temp->fracLeft -= runTime;
        lastStop = current_time;

        if ( temp->fracLeft == 0 )
        {
            numberDiskInt++;
            uint32_t ppn = DRAMadd( temp, vpn );
            addPTE( temp, vpn, ppn );

            gll_pop(blockedProcess);
            gll_pushBack(readyProcess, temp);

            temp->blockedStateDuration += current_time - temp->blockOccur;
            totalBlockedDuration += current_time - temp->blockOccur;

            current_time += sysParam->Swap_interrupt_handling_time;
            OSTime += sysParam->Swap_interrupt_handling_time;
            temp->OS_time += sysParam->Swap_interrupt_handling_time;
        }
    }

    if(debug == 1)
        printf("Done diskToMemory\n");
}


void simulate()
{
    init();
    statsinit();

    //get the first ready process to running state
    struct PCB* temp = gll_first(readyProcess);
    gll_pushBack(runningProcess, temp);
    gll_pop(readyProcess);

    struct PCB* temp2 = gll_first(runningProcess);
    gll_pushBack(resultStats.executionOrder, temp2->name);
    previousProcess = temp2;

    while(1)
    {
        //execute the running process until an interrupt occurs or the process finishes
        int simPause = processSimulator();

        //Update the time of the next time interrupt
        while ( current_time >= nextQuanta )
            nextQuanta += quantum;

        //transfer arrived processes from processList to readyProcess list
        struct PCB* temp = gll_first(processList);
        
        while((temp!= NULL) && ( temp->start_time <= current_time))
        {
            temp->memoryFile = openTrace(temp->memoryFilename);
            temp->numOfIns = readNumIns(temp->memoryFile);

            struct NextMem* tempAddr = readNextMem(temp->memoryFile);

            while(tempAddr!= NULL)
            {
                gll_pushBack(temp->memReq, tempAddr);
                tempAddr = readNextMem(temp->memoryFile);
            }
            gll_pushBack(readyProcess, temp);
            gll_pop(processList);

            temp = gll_first(processList);
        }

        //move elements from disk to memory
        diskToMemory();

        //This memory trace done
        if(simPause == -1)
        {
            //finish up this process
            cleanUpProcess(gll_first(runningProcess));
            gll_pop(runningProcess);
            previousProcess = NULL;
        }

        //move running process to readyProcess list
        int runningProcessNUll = 0;
        if(simPause == 1 || simPause == 4)
        {
            if(gll_first(runningProcess) != NULL)
            {
                gll_pushBack(readyProcess, gll_first(runningProcess));
                gll_pop(runningProcess);
            }
            else{
                runningProcessNUll = 1;
            }
            if(simPause == 1)
            {
                nextQuanta = current_time + quantum;
            }
        }

        //Run the scheduler
        schedulingRR(simPause);

        //move elements from disk to memory
        diskToMemory();

        //Nothing in running or ready. need to increase time to next timestamp when a process becomes ready.
        if((gll_first(runningProcess) == NULL) && (gll_first(readyProcess) == NULL))
        {
            if(debug == 1)
            {
                printf("\nNothing in running or ready\n");
            }
            if((gll_first(blockedProcess) == NULL) && (gll_first(processList) == NULL))
            {
                if(debug == 1)
                {
                    printf("\nAll done\n");
                }
                break;
            }
            struct PCB* tempProcess = gll_first(processList);
            struct PCB* tempBlocked = gll_first(blockedProcess);

            //TODO: Set correct value of timeOfNextPendingDiskInterrupt
            uint64_t timeOfNextPendingDiskInterrupt = 0;
            if ( tempBlocked )
            {
                timeOfNextPendingDiskInterrupt = current_time + tempBlocked->fracLeft;
            }

            if(tempBlocked == NULL)
            {
                if(debug == 1)
                {
                    printf("\nGoing to move from proess list to ready\n");
                }
                struct NextMem* tempAddr;
                tempProcess->memoryFile = openTrace(tempProcess->memoryFilename);
                tempProcess->numOfIns = readNumIns(tempProcess->memoryFile);
                tempAddr = readNextMem(tempProcess->memoryFile);
                while(tempAddr!= NULL)
                {
                    gll_pushBack(tempProcess->memReq, tempAddr);
                    tempAddr = readNextMem(tempProcess->memoryFile);
                }
                gll_pushBack(readyProcess, tempProcess);
                gll_pop(processList);
                
                while(nextQuanta < tempProcess->start_time)
                {   
                    current_time = nextQuanta;
                    nextQuanta = current_time + quantum;
                }
                OSTime += (tempProcess->start_time-current_time);
                current_time = tempProcess->start_time; 
            }
            else
            {
                if(tempProcess == NULL)
                {
                    if(debug == 1)
                    {
                        printf("\nGoing to move from blocked list to ready\n");
                    }
                    OSTime += (timeOfNextPendingDiskInterrupt-current_time);
                    current_time = timeOfNextPendingDiskInterrupt;
                    while (nextQuanta < current_time)
                    {
                        nextQuanta = nextQuanta + quantum;
                    }
                    diskToMemory();
                }
                else if(tempProcess->start_time >= timeOfNextPendingDiskInterrupt)
                {
                    if(debug == 1)
                    {
                        printf("\nGoing to move from blocked list to ready\n");
                    }
                    OSTime += (timeOfNextPendingDiskInterrupt-current_time);
                    current_time = timeOfNextPendingDiskInterrupt;
                    while (nextQuanta < current_time)
                    {
                        nextQuanta = nextQuanta + quantum;
                    }
                    diskToMemory();
                }
                else{
                    struct NextMem* tempAddr;
                    if(debug == 1)
                    {
                        printf("\nGoing to move from proess list to ready\n");
                    }
                    tempProcess->memoryFile = openTrace(tempProcess->memoryFilename);
                    tempProcess->numOfIns = readNumIns(tempProcess->memoryFile);
                    tempAddr = readNextMem(tempProcess->memoryFile);
                    while(tempAddr!= NULL)
                    {
                        gll_pushBack(tempProcess->memReq, tempAddr);
                        tempAddr = readNextMem(tempProcess->memoryFile);
                    }
                    gll_pushBack(readyProcess, tempProcess);
                    gll_pop(processList);
                    
                    while(nextQuanta < tempProcess->start_time)
                    {   
                        current_time = nextQuanta;
                        nextQuanta = current_time + quantum;
                    }
                    OSTime += (tempProcess->start_time-current_time);
                    current_time = tempProcess->start_time; 
                }
            }   
        }
    }
}

int main(int argc, char** argv)
{
    //Check inputs
    if(argc == 1)
    {
        printf("No file input\n");
        exit(1);
    }
    traceFileName = argv[1]; //of the form inputx.txt
    outputFileName = argv[2]; //of the form outputx.txt

    //Set the output file name
    char *txtExtension = ".txt";
    char *LRUExtension = "_LRU";
    char *clockExtension = "_clock";
    char output_LRU[ strlen(outputFileName) + strlen(LRUExtension) + 1 ];
    char output_clock[ strlen(outputFileName) + strlen(clockExtension) + 1 ];
    memcpy( output_LRU, outputFileName, strlen(outputFileName) - strlen(txtExtension) );
    memcpy( output_clock, outputFileName, strlen(outputFileName) - strlen(txtExtension) );
    memcpy( output_LRU + strlen(outputFileName) - strlen(txtExtension), LRUExtension, strlen(LRUExtension) );
    memcpy( output_clock + strlen(outputFileName) - strlen(txtExtension), clockExtension, strlen(clockExtension) );
    memcpy( output_LRU + strlen(outputFileName) - strlen(txtExtension) + strlen(LRUExtension), txtExtension, strlen(txtExtension) );
    memcpy( output_clock + strlen(outputFileName) - strlen(txtExtension) + strlen(clockExtension), txtExtension, strlen(txtExtension) );
    output_LRU[ strlen(outputFileName) + strlen(LRUExtension) ] = '\0';
    output_clock[ strlen(outputFileName) + strlen(clockExtension) ] = '\0';

    //Start the simulation
    page_replacement_policy = 0;
    printf( "Page replacement policy is LRU:\n" );
    simulate();
    finishAll();
    statsUpdate();

    if(writeToFile(output_LRU, resultStats) == 0)
    {
        printf("Could not write output to file\n");
    }
    printf("Start time = %llu, \tEnd time =%llu\n", resultStats.start_time, resultStats.end_time);
    printf("Total number of context switches = %d \n", resultStats.numberOfContextSwitch );
    printf("Total number of disk interrupts = %d \n", resultStats.numberOfDiskInt );
    printf("Total number of TLB misses = %d \n", resultStats.totalTLBmiss );
    printf("%% of TLB misses = %lf \n", 100.0 * resultStats.totalTLBmiss / (resultStats.totalTLBhit + resultStats.totalTLBmiss) );
    printf("Total number of page faults = %d \n", resultStats.totalPgFaults );
    printf("%% of page faults = %lf \n", 100.0 * resultStats.totalPgFaults / (resultStats.totalPgHits + resultStats.totalPgFaults) );
    printf("Total blocked state duration = %llu \n", resultStats.totalBlockedStateDuration );
    printf("Total amount of time spent in OS mode = %llu \n", resultStats.OSModetime );
    printf("Total amount of time spent in user mode = %llu \n", resultStats.userModeTime );
    gll_each(resultStats.perProcessStats, &printStats);

    printf( "\n--------------------------------------------------\n\n");

    page_replacement_policy = 1;
    clockHand = 0;
    printf( "Page replacement policy is clock algorithm:\n" );
    simulate();
    finishAll();
    statsUpdate();

    if(writeToFile(output_clock, resultStats) == 0)
    {
        printf("Could not write output to file\n");
    }
    printf("Start time = %llu, \tEnd time =%llu\n", resultStats.start_time, resultStats.end_time);
    printf("Total number of context switches = %d \n", resultStats.numberOfContextSwitch );
    printf("Total number of disk interrupts = %d \n", resultStats.numberOfDiskInt );
    printf("Total number of TLB misses = %d \n", resultStats.totalTLBmiss );
    printf("%% of TLB misses = %lf \n", 100.0 * resultStats.totalTLBmiss / (resultStats.totalTLBhit + resultStats.totalTLBmiss) );
    printf("Total number of page faults = %d \n", resultStats.totalPgFaults );
    printf("%% of page faults = %lf \n", 100.0 * resultStats.totalPgFaults / (resultStats.totalPgHits + resultStats.totalPgFaults) );
    printf("Total blocked state duration = %llu \n", resultStats.totalBlockedStateDuration );
    printf("Total amount of time spent in OS mode = %llu \n", resultStats.OSModetime );
    printf("Total amount of time spent in user mode = %llu \n", resultStats.userModeTime );
    gll_each(resultStats.perProcessStats, &printStats);
    printf( "\n");
}