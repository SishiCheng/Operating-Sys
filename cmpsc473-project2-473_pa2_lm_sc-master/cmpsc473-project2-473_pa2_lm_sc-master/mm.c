/*
 * File       : mm.c
 *
 * Description: We use segregate free lists and slab allocation with bitmaps to complete each memory allocation task. 
 * 
 * For malloc(), if the required size is less than or equal to 32, we will mainly allocate the payload in slabs. We 
 * have two types of slabs: one for payload of size 16, and one for payload of size 32. At the beginning of each 
 * slab, there is a bitmap indicating which locations are free. In this way, the payloads in slabs do not need header
 * and footer, and this help us decrease internal fragmentation. If the required size in malloc() is greater than 32,
 * we will allocate the payload using the segregate free lists. We have 36 free lists which are for free blocks of 
 * sizes within 2^5, 2^6, 2^7, ... , 2^40 respectively. According to the required payload size, we will search through
 * the corresponding free list and find a best fit for the payload. Each block in the free lists will have its first
 * 24 bytes as the following: 8 bytes for header, 8 bytes for the address of the predecessor, 8 bytes for address of
 * the successor, and it will also have its last 8 bytes for a footer. After the block is allocated, the information
 * about predecessor and successor will be eliminated. If we fail to allocate the payload because there is not enough
 * space in the heap, we will increase the size of the heap and then allocate the payload. 
 *
 * For free(), we will first check if the given payload is allocated in slabs. If it does, we will change the bitmap
 * of the corresponding slab to mark the payload location as free. If the given payload is not allocated in slabs, we
 * will add it into a free list according to its size.
 *
 * For realloc(), we will check if the old size is equal to the new size. If it does, we will do nothing and just 
 * return the old pointer. If the old size is greater than the new size, we will free the redundant space at the end 
 * using the free() function. If the old size is smaller than the new size, we will copy the content of the payload,
 * free the old pointer, allocate a new payload of the new size, and paste the content to the new payload.
 *
 * Author     : Leran Ma, Sishi Cheng
 *
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#include "mm.h"
#include "memlib.h"

/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
// #define DEBUG

#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16

// Functions
static bool in_heap( const void *p );
void free( void *ptr );
static void addTags( char *block,
                     uint8_t valid,
                     size_t fullSize,
                     char *pred,
                     char *succ );

char **lists; // The segregate free lists
char **slabs16; // The slab for the 16-bytes's block
char **slabs32; // The slab for the 32-bytes's block

////////////////////////////////////////////////////////////////////////////////
//
// Function     : listDelete
// Description  : Delete the segregate free list
//
// Inputs       : pred - the predecessor of the list that needs to be deleted
//                succ - the successor of the list that needs to be deleted
//                index - the index of the list 
// Outputs      : nothing
static void listDelete( char *pred, char *succ, uint8_t index )
{
    // Check whether the predecessor is NULL
    if ( !pred )
    {
        lists[index] = succ;
        addTags( succ, 2, 0, NULL, (char *)1 );
    }
    else
    {
        addTags( pred, 2, 0, (char *)1, succ );
        addTags( succ, 2, 0, pred, (char *)1 );
    }
    return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : listAdd
// Description  : Add the segregate free list
//
// Inputs       : addr - the address of the block
//                index - the index of the list 
// Outputs      : nothing
static void listAdd( char *addr, uint8_t index )
{
    addTags( addr, 2, 0, (char *)1, lists[index] );
    addTags( lists[index], 2, 0, addr, (char *)1 );
    lists[index] = addr;
    return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : getIndedx
// Description  : Calculate the index of the block
//
// Inputs       : size - the size of the block
// Outputs      : The index of the block
static uint8_t getIndex( size_t size )
{
    return (uint8_t) ceil( log2(size) ) - 5;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : getBlockInfo
// Description  : Get the information of the block
//
// Inputs       : blockHead - the header of the block
//                blockFoot - the footer of the block
//                isValid - whether the block is free
//                size - the size of the block
//                pred - the predecessor of the block
//                succ - the successor of the block
// Outputs      : nothing
static void getBlockInfo( char *blockHead,
                          char *blockFoot,
                          uint8_t *isValid,
                          size_t *size,
                          char **pred,
                          char **succ )
{
    uint64_t header;
    char *pred_tmp = NULL; // The temporary pointer for predecessor
    char *succ_tmp = NULL; // The temporary pointer for successor

    // Check the blockHead is not null
    if ( blockHead )
    {
        mem_memcpy( &header, blockHead, sizeof(header) );

        // Check the block is free 
        if ( isValid )
            *isValid = header & 7;

        // Check the size is not null
        if ( size )
            *size = header >> 3;

        // Check the block is not free and the size is greater than ALIGNMENT
        if ( !(header & 7) && (header >> 3) > ALIGNMENT )
        {
            // Check the predecessor is not null
            if ( pred )
            {
                mem_memcpy( &pred_tmp, blockHead + ALIGNMENT/2, sizeof( pred_tmp ) );
                *pred = pred_tmp;
            }

            // Check the successor is not null
            if ( succ )
            {
                mem_memcpy( &succ_tmp, blockHead + ALIGNMENT, sizeof( succ_tmp ) );
                *succ = succ_tmp;
            }
        }
    }
    else if ( blockFoot )
    {
        mem_memcpy( &header, blockFoot - sizeof(header), sizeof(header) );

        // Check the block is free
        if ( isValid )
            *isValid = header & 7;

        // Check the size of the block is not null
        if ( size )
            *size = header >> 3;

        // Check the block is not free and the size is greater than ALIGNMENT
        if ( !(header & 7) && (header >> 3) > ALIGNMENT )
        {
            // Check the predecessor is not null
            if ( pred )
            {
                mem_memcpy( &pred_tmp, blockFoot - (header >> 3) + ALIGNMENT/2, sizeof( pred_tmp ) );
                *pred = pred_tmp;
            }
            // Check the successor is not null
            if ( succ )
            {
                mem_memcpy( &succ_tmp, blockFoot - (header >> 3) + ALIGNMENT, sizeof( succ_tmp ) );
                *succ = succ_tmp;
            }
        }
    }
    return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : addTags
// Description  : Add the tags for the block
//
// Inputs       : block - the block
//                valid - the status of whether the block is valid
//                fullsize - the size of the block
//                pred - the predecessor of the block
//                succ - the successor of the block
// Outputs      : nothing
static void addTags( char *block,
                     uint8_t valid,
                     size_t fullSize,
                     char *pred,
                     char *succ )
{
    // Check the block is not null
    if ( !block )
        return;

    uint64_t header = fullSize << 3; // The header of the block

    // Check the block is not free
    if ( valid == 1 )
    {
        header++;
        pred = NULL;
        succ = NULL;
        mem_memcpy( block, &header, sizeof(header) );
        mem_memcpy( block + fullSize - ALIGNMENT/2, &header, sizeof(header) );

        // Check the block size is greater than 16
        if ( fullSize > ALIGNMENT )
        {
            mem_memcpy( block + ALIGNMENT/2, &pred, sizeof(pred) );
            mem_memcpy( block + ALIGNMENT, &succ, sizeof(succ) );
        }
    }
    else if ( valid == 0 )
    {
        mem_memcpy( block, &header, sizeof(header) );       
        mem_memcpy( block + fullSize - ALIGNMENT/2, &header, sizeof(header) );

        // Check the block size is greater than 16
        if ( fullSize > ALIGNMENT )
        {
            mem_memcpy( block + ALIGNMENT/2, &pred, sizeof(pred) );
            mem_memcpy( block + ALIGNMENT, &succ, sizeof(succ) );
        }
    }
    else if ( valid == 2 )
    {
        // Check the predecessor is 1
        if ( pred == (char *)1 )
            mem_memcpy( block + ALIGNMENT, &succ, sizeof(succ) );
        else if ( succ == (char *)1 )
            mem_memcpy( block + ALIGNMENT/2, &pred, sizeof(pred) );
    }
    return;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : findFirst0
// Description  : Find the first free block in the slab
//
// Inputs       : num - a portion of a bitmap
// Outputs      : the index of the first free block in the slab
static uint64_t findFirst0( uint64_t num )
{
    if ( (num >> 32) )
        return floor( log2( num >> 32 ) ) + 32;
    else
        return floor( log2( num << 32 >> 32 ) );
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : slabAdd
// Description  : Add a block in a slab
//
// Inputs       : size - the size of the block
// Outputs      : the address of the block or NULL
static char *slabAdd( size_t size )
{
    // Check the block size is 16
    if ( size == ALIGNMENT )
    {
        bool toBeAlloc = false; // The boolean variable to show the allocate status of the block

        // Loop through the slab16
        for ( int i = 9; i > -1; --i )
        {
            // Find the block in slab16 is not null
            if ( slabs16[i] )
            {
                int num = 0; // Number of maps 

                // Check the index is greater than 8
                if ( i > 8 )
                    num = 32;
                else
                    num = 8;
                uint64_t maps[num]; // The bitmap

                // Loop through the maps
                for (int j = 0; j < num; ++j)
                {
                    mem_memcpy( &maps[j], slabs16[i] + j*ALIGNMENT/2, sizeof(maps[j]) );

                    // Check the bitmap is full
                    if ( maps[j] != ( maps[j] | -1 ) )
                    {
                        uint64_t first0 = findFirst0( ( maps[j] | -1 ) - maps[j] ); // The index of the first free block
                        maps[j] += ( (maps[j] & 0) | 1 ) << first0;
                        mem_memcpy( slabs16[i] + j*ALIGNMENT/2, &maps[j], sizeof(maps[j]) );
                        return slabs16[i] + (num+1)/2*ALIGNMENT + j*1024 + (63-first0)*ALIGNMENT;
                    }
                }
            }
            else toBeAlloc = true;
        }

        // Check the block will be allocate
        if ( toBeAlloc )
        {
            // Loop through the slabs16
            for (int i = 0; i < 10; ++i)
            {
                // Find the first null blook in slabs16
                if ( !slabs16[i] )
                {
                    int num = ((i&0)|1) << i; // Number of the size

                    // Check the index is greater than 8
                    if ( i > 8 )
                        num = 32;
                    else
                        num = 8;
                    slabs16[i] = malloc( num*1024 + num * ALIGNMENT/2 );
                    uint64_t map = 1; // The bitmap
                    map = map << 63;
                    mem_memcpy( slabs16[i], &map, sizeof(map) );
                    map = 0;

                    // Allocate the memory 
                    for ( int j = 1; j < num; ++j )
                        mem_memcpy( slabs16[i] + j*ALIGNMENT/2, &map, sizeof(map) );
                    return slabs16[i] + (num+1)/2 * ALIGNMENT;
                }
            }
        }
    }

    else if ( size == 2*ALIGNMENT )
    {
        bool toBeAlloc = false; // // The boolean variable to show the allocate status of the block

        // Loop through the slab32
        for ( int i = 13; i > -1; --i )
        {
            // Find the block in slab16 is not null
            if ( slabs32[i] )
            {
                int num = ( (i & 0) | 1 ) << i; // Number of maps

                // Check the index is greater than 8
                if ( i > 8 )
                    num = 32;
                else
                    num = 8;
                uint64_t maps[num]; // The bitmap

                // Loop through the maps
                for (int j = 0; j < num; ++j)
                {
                    mem_memcpy( &maps[j], slabs32[i] + j*ALIGNMENT/2, sizeof(maps[j]) );

                    // Check the bitmap is full
                    if ( maps[j] != ( maps[j] | -1 ) )
                    {
                        uint64_t first0 = findFirst0( ( maps[j] | -1 ) - maps[j] ); // The index of the first free block
                        maps[j] += ( (maps[j] & 0) | 1 ) << first0;
                        mem_memcpy( slabs32[i] + j*ALIGNMENT/2, &maps[j], sizeof(maps[j]) );
                        return slabs32[i] + (num+1)/2*ALIGNMENT + j*2048 + (63-first0)*2*ALIGNMENT;
                    }
                }
            }
            else toBeAlloc = true;
        }

        // Check the block will be allocate
        if ( toBeAlloc )
        {
            // Loop through the slab32
            for (int i = 0; i < 14; ++i)
            {
                // Find the first null blook in slabs32
                if ( !slabs32[i] )
                {
                    int num = ((i&0)|1) << i; // The number of the maps

                    // Check the index is greater than 8
                    if ( i > 8 )
                        num = 32;
                    else
                        num = 8;
                    slabs32[i] = malloc( num*2048 + num * ALIGNMENT/2 );
                    uint64_t map = 1; // The bitmap
                    map = map << 63;
                    mem_memcpy( slabs32[i], &map, sizeof(map) );
                    map = 0;

                    // Allocate the memory 
                    for ( int j = 1; j < num; ++j )
                        mem_memcpy( slabs32[i] + j*ALIGNMENT/2, &map, sizeof(map) );
                    return slabs32[i] + (num+1)/2 * ALIGNMENT;
                }
            }
        }
    }

    return NULL;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : slabDelete
// Description  : Delete a block in a slab
//
// Inputs       : addr - the address of the block that needs to be deleted
//                size - the size of the block that needs to be deleted
//                index - the index of the block that needs to be deleted
// Outputs      : nothing
static void slabDelete( char *addr, uint8_t size, uint8_t index )
{
    // Check the size of the block is 16
    if ( size == ALIGNMENT )
    {
        uint64_t num = 1; // the number of maps 
        num = num << index;

        // Check the index is greater than 8
        if ( index > 8 )
            num = 32;
        else
            num = 8;
        uint64_t mapIndex = ( addr - slabs16[index] - (num+1)/2*ALIGNMENT )/1024; // The index of a bitmap
        uint64_t tmp = 1; // The temporary variable
        tmp = tmp << (63 - ( addr - slabs16[index] - (num+1)/2*ALIGNMENT - mapIndex*1024 )/16 );
        uint64_t map = 0; // The bitmap
        mem_memcpy( &map, slabs16[index] + mapIndex*ALIGNMENT/2, sizeof(map) );
        map -= tmp;
        mem_memcpy( slabs16[index] + mapIndex*ALIGNMENT/2, &map, sizeof(map) );

        bool isFree = true; // The boolean variable to record the status

        // Loop through the bitmap
        for ( int i = 0; i < (int)num; ++i )
        {
            mem_memcpy( &map, slabs16[index] + i*ALIGNMENT/2, sizeof(map) );

            // Check whether the map is not free
            if ( map )
            {
                isFree = false;
                break;
            }
        }
        // Check the isFree boolean variable is free
        if ( isFree )
        {
            free( slabs16[index] );
            slabs16[index] = NULL;
        }
        return;
    }
    else if ( size == 2*ALIGNMENT )
    {
        uint64_t num = 1; // the number of bitmap
        num = num << index;

        // Check the index is greater than 8
        if ( index > 8 )
            num = 32;
        else
            num = 8;
        uint64_t mapIndex = ( addr - slabs32[index] - (num+1)/2*ALIGNMENT )/2048; // The index of a bitmap
        uint64_t tmp = 1; // The temporary variable
        tmp = tmp << (63 - ( addr - slabs32[index] - (num+1)/2*ALIGNMENT - mapIndex*2048 )/32 );
        uint64_t map = 0; // The bitmap
        mem_memcpy( &map, slabs32[index] + mapIndex*ALIGNMENT/2, sizeof(map) );
        map -= tmp;
        mem_memcpy( slabs32[index] + mapIndex*ALIGNMENT/2, &map, sizeof(map) );

        bool isFree = true; // The boolean variable to record the status

        // Loop through the bitmap
        for ( int i = 0; i < (int)num; ++i )
        {
            mem_memcpy( &map, slabs32[index] + i*ALIGNMENT/2, sizeof(map) );

            // Check whether the map is not free
            if ( map )
            {
                isFree = false;
                break;
            }
        }
        // Check the isFree boolean variable is free
        if ( isFree )
        {
            free( slabs32[index] );
            slabs32[index] = NULL;
        }
        return;
    }
    return;
}

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT-1)/ALIGNMENT);
}

/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init( void )
{
    /* IMPLEMENT THIS */
    lists = mem_sbrk( ALIGNMENT/2*36 );
    slabs16 = mem_sbrk( ALIGNMENT/2*10 );
    slabs32 = mem_sbrk( ALIGNMENT/2*15 );

    // Initialize the segregate free list
    for ( int i = 0; i < 36; ++i )
    {
        lists[i] = NULL;
    }

    // Initializa the slabs
    for ( int i = 0; i < 10; ++i )
    {
        slabs16[i] = NULL;
    }

    for ( int i = 0; i < 14; ++i )
    {
        slabs32[i] = NULL;
    }


    return true;
}

/*
 * malloc
 */
void *malloc( size_t size )
{
    /* IMPLEMENT THIS */
    char *ptr = NULL; // A pointer to save the address

    // Check the size of the block is less than or equal to 32
    if ( size <= ALIGNMENT )
        ptr = slabAdd( ALIGNMENT );
    else if ( size <= 2*ALIGNMENT )
        ptr = slabAdd( 2*ALIGNMENT );

    // Check the pointer is not null
    if ( ptr )
        return ptr;

    size_t newsize = align(size) + ALIGNMENT; // The size of the block
    uint8_t index = getIndex( newsize ); // The index of the block in segregate free list

    // Loop through the free lists and find the best fit
    for ( int i = index; i < 36; ++i )
    {
        uint64_t difference = (1ull*(1ull<<40)); // The difference of the size
        char *best = NULL; // The pointer to save the best block
        size_t bestSize = 0; // The best block's size
        char *bestPred = NULL; // The best block's predecessor
        char *bestSucc = NULL; // The best block's successor
        ptr = lists[i]; 

        // Check the pointer is not null
        while ( ptr )
        {
            uint8_t isValid = 1; // the status of valid
            size_t size = 0; // the size of the block that the pointer points to 
            char *pred = NULL;// the predecessor of the block that the pointer points to
            char *succ = NULL;// the successor of the block that the pointer points to
            getBlockInfo( ptr, NULL, &isValid, &size, &pred, &succ );

            // Chenck the block is free and the size is greater than the new size
            if ( !isValid && size >= newsize )
            {
                // Check the difference of the sizes is less than the difference that recorded before
                if ( size - newsize < difference )
                {
                    difference = size - newsize;
                    best = ptr;
                    bestSize = size;
                    bestPred = pred;
                    bestSucc = succ;
                }

                // Check the difference is 0
                if ( !difference )
                    break;
            }
            ptr = succ;
        }

        // Check the best pointer is not null
        if ( best )
        {
            listDelete( bestPred, bestSucc, i );
            addTags( best, 1, newsize, NULL, NULL );

            // Check the differences of the bestSize and the newsize is greater than or equal to 32
            if ( bestSize - newsize >= 2*ALIGNMENT )
            {
                addTags( best + newsize, 0, difference, NULL, NULL );
                listAdd( best + newsize, getIndex( difference ) );
            }
            else if ( bestSize > newsize )
            {
                addTags( best + newsize, 0, difference, NULL, NULL );
            }
            return best + ALIGNMENT/2;
        }
    }

    ptr = mem_sbrk( newsize ) + ALIGNMENT/2;
    addTags( ptr - ALIGNMENT/2, 1, newsize, NULL, NULL );
    return ptr;
}

/*
 * free
 */
void free( void *ptr )
{
    /* IMPLEMENT THIS */
    // Check the pointer is not null
    if ( !ptr )
        return;

    // Loop through the slab16
    for (int i = 0; i < 10; ++i)
    {
        int num = ( (i & 0) | 1 ) << i; // The bymber of bitmap

        // Check the index is greater than 8
        if ( i > 8 )
            num = 32;
        else
            num = 8;

        // If ptr points to a location in a slab
        if ( (char *)ptr > slabs16[i] &&
             (char *)ptr < slabs16[i] + num*1024 + num * ALIGNMENT/2 )
        {
            slabDelete( ptr, ALIGNMENT, i );
            return;
        }
    }

    // Loop through the slab32
    for (int i = 0; i < 14; ++i)
    {
        int num = ( (i & 0) | 1 ) << i; // The bumber of bitmap

        // Check the index is greater than 8
        if ( i > 8 )
            num = 32;
        else
            num = 8;

        // If ptr points to a location in a slab
        if ( (char *)ptr > slabs32[i] &&
             (char *)ptr < slabs32[i] + num*2048 + num * ALIGNMENT/2 )
        {
            slabDelete( ptr, 2*ALIGNMENT, i );
            return;
        }
    }

    uint8_t isValid = 1; // The valid status of the block that ptr points to
    size_t size = 0; // The size of the block that ptr points to
    getBlockInfo( (char *)ptr - ALIGNMENT/2, NULL, &isValid, &size, NULL, NULL );

    // Check the ptr is free
    if ( isValid != 1 )
        return;

    uint8_t isPreValid = 1; // The valid status of the previous block that ptr points to
    size_t preSize = 0; // The size of the previous block
    char *prePred = NULL; // The predecessor of the previous block
    char *preSucc = NULL; // The successor of the previous block

    // Check the previous block is in heap
    if ( in_heap( (char *)ptr - ALIGNMENT -1 ) )
    {
        getBlockInfo( NULL, (char *)ptr - ALIGNMENT/2, &isPreValid, &preSize,
                      &prePred, &preSucc );
    }

    uint8_t isPostValid = 1; // The valid status of the post block that ptr points to
    size_t postSize = 0; // The size of the post block
    char *postPred = NULL; // The predecessor of the post block
    char *postSucc = NULL; // The successor of the post block

    // Check the post block is in heap
    if ( in_heap( (char *)ptr + size ) )
    {
        getBlockInfo( (char *)ptr + size - ALIGNMENT/2, NULL, &isPostValid, &postSize,
                      &postPred, &postSucc );
    }
        
    // Check the previous block is free
    if ( isPreValid == 0 )
    {
        //Check the post block is free
        if ( isPostValid == 0 )
        {
            size += postSize;

            // Check the size of the post block is less than16
            if ( postSize > ALIGNMENT )
            {
                listDelete( postPred, postSucc, getIndex(postSize) );
                getBlockInfo( NULL, (char *)ptr - ALIGNMENT/2, &isPreValid, &preSize,
                    &prePred, &preSucc );
            }
        }

        size += preSize;

        // Check the size of the previous block is greater than 16
        if ( preSize > ALIGNMENT )
            listDelete( prePred, preSucc, getIndex(preSize) );

        addTags( (char *)ptr - ALIGNMENT/2 - preSize, 0, size, NULL, NULL );
        listAdd( (char *)ptr - ALIGNMENT/2 - preSize, getIndex(size) );       
    }
    else if ( isPostValid == 0 )
    {
        size += postSize;

        // Check the size of the post block is greater than 16
        if ( postSize > ALIGNMENT )
            listDelete( postPred, postSucc, getIndex(postSize) );

        addTags( (char *)ptr - ALIGNMENT/2, 0, size, NULL, NULL );
        listAdd( (char *)ptr - ALIGNMENT/2, getIndex(size) );
    }
    else if ( isPreValid == 1 && isPostValid == 1 )
    {
        addTags( (char *)ptr - ALIGNMENT/2, 0, size, NULL, NULL );

        // Check the size of the block is greater than 16
        if ( size > ALIGNMENT )
            listAdd( (char *)ptr - ALIGNMENT/2, getIndex(size) );
    }

    return;
}

/*
 * realloc
 */
void *realloc( void *oldptr, size_t size )
{
    /* IMPLEMENT THIS */    
    if ( !in_heap(oldptr) )
        return NULL;

    // If oldptr points to a location in the slabs, 
    // manipulate the bitmap of the slab correspondingly and reallocate the payload.
    for (int i = 0; i < 10; ++i)
    {
        int num = ( (i & 0) | 1 ) << i;
        if ( i > 8 )
            num = 32;
        else
            num = 8;
        if ( (char *)oldptr > slabs16[i] &&
             (char *)oldptr < slabs16[i] + num*1024 + num * ALIGNMENT/2 )
        {
            if ( size <= ALIGNMENT )
                return oldptr;

            slabDelete( oldptr, ALIGNMENT, i );
            char *ptr = malloc( size );
            mem_memcpy( ptr, oldptr, ALIGNMENT );
            return ptr;
        }
    }

    for (int i = 0; i < 14; ++i)
    {
        int num = ( (i & 0) | 1 ) << i;
        if ( i > 8 )
            num = 32;
        else
            num = 8;
        if ( (char *)oldptr > slabs32[i] &&
             (char *)oldptr < slabs32[i] + num*2048 + num * ALIGNMENT/2 )
        {
            if ( size > ALIGNMENT && size <= ALIGNMENT*2 )
                return oldptr;

            slabDelete( oldptr, 2*ALIGNMENT, i );
            char *ptr = malloc( size );
            if ( size <= ALIGNMENT )
                mem_memcpy( ptr, oldptr, ALIGNMENT );
            else
                mem_memcpy( ptr, oldptr, 2*ALIGNMENT );

            return ptr;
        }
    }

    // If oldptr does not point to a location in the slabs:
    uint8_t isOldValid = 1;
    size_t oldsize = 0;
    getBlockInfo( (char *)oldptr - ALIGNMENT/2, NULL, &isOldValid, &oldsize, NULL, NULL );

    if ( isOldValid != 1 )
        return NULL;

    size_t newsize = align(size) + ALIGNMENT;
    
    // If the old size is the same as the new size, do nothing.
    if ( oldsize == newsize )
        return oldptr;

    // If the old size is greater than the new size, 
    // free the redundant space at the end using the free() function.
    else if ( oldsize > newsize )
    {
        char temp[size];
        mem_memcpy( temp, oldptr, size );
        addTags( (char *)oldptr - ALIGNMENT/2, 1, newsize, NULL, NULL );
        addTags( (char *)oldptr - ALIGNMENT/2 + newsize, 1, oldsize - newsize,
            NULL, NULL );
        free( (char *)oldptr + newsize );
        mem_memcpy( oldptr, temp, size );
        return oldptr;
    }

    // If the old size is smaller than the new size, 
    // copy the content of the payload, free the old pointer, 
    // allocate a new payload of the new size, and paste the content to the new payload.
    else
    {
        char temp[oldsize - ALIGNMENT];
        mem_memcpy( temp, oldptr, oldsize - ALIGNMENT );
        free( oldptr );
        char *ptr = malloc( size );
        mem_memcpy( ptr, temp, oldsize - ALIGNMENT );
        return ptr;
    }
}

/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}

/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 * Check the heap for the followings:
 *   1. Is every payload aligned with 16?
 *   2. Are there contiguous free blocks that escape coalescing?
 *   3. Is there any free block not in free lists?
 *   4. Is every free block actually in free lists?
 *   5. Is every block in the free lists actually free?
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
    char *preBlock = NULL;
    char *ptr = mem_heap_lo() + ALIGNMENT/2*(36+12+13); // make ptr points to the first block
    uint8_t isPreValid = 1;
    uint8_t isValid = 1;
    size_t size = 0;
    while ( ptr < (char *)mem_heap_hi() )
    {
        // Is every payload aligned with 16?
        if ( !aligned( ptr + ALIGNMENT/2 ) )
        {
            fprintf( stderr, "Payload of the block %p is not in aligned.\n", ptr );
            return false;
        }

        // Are there contiguous free blocks that escape coalescing?
        getBlockInfo( ptr, NULL, &isValid, &size, NULL, NULL );
        if ( isPreValid == isValid && isPreValid == 0 )
        {
            fprintf( stderr, "Contiguous free blocks %p and %p escape coalescing.\n",
                preBlock, ptr );
            return false;
        }

        // Is every free block actually in free lists?
        if ( !isValid && size > ALIGNMENT )
        {
            bool found = false;
            char *tmp = lists[getIndex(size)];
            char *succ = NULL;
            while ( tmp )
            {
                getBlockInfo( tmp, NULL, NULL, NULL, NULL, &succ );
                if ( tmp == ptr )
                {
                    found = true;
                    break;
                }
                tmp = succ;
            }
            if ( !found )
            {
                fprintf( stderr, "Free block %p is not in free lists.\n", ptr );
                return false;
            }
        }
        isPreValid = isValid;
        preBlock = ptr;
        ptr += size;
    }

    char *succ = NULL;

    for ( int i = 0; i < 36; ++i )
    {
        ptr = lists[i];
        while ( ptr )
        {
            // Is every block in the free lists actually free?
            getBlockInfo( ptr, NULL, &isValid, NULL, NULL, &succ );
            if ( isValid )
            {
                fprintf( stderr, "Block %p in free list %d is not marked free.\n", ptr, i );
                return false;
            }
            ptr = succ;
        }
    }

#endif /* DEBUG */
    return true;
}
