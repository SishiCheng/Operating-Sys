#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <search.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include "helper.h"


// Defines possible return values from buffer functions
enum buffer_status {
    buffer_EMPTY = 0,
    buffer_FULL = 0,
    BUFFER_SUCCESS = 1,
    CLOSED_ERROR = -2,
    BUFFER_ERROR = -1,
    DESTROY_ERROR = -3,
    BUFFER_SPECIAL_MESSSAGE = -4
};

enum buffer_status buffer_top_message (state_t* buffer, void **data,int index);
// Creates a buffer with the given capacity
state_t* buffer_create(int capacity);

// Adds the value into the bufferQ
// Returns BUFFER_SUCCESS if the bufferQ is not full and value was added
// Returns BUFFER_ERROR otherwise
enum buffer_status buffer_add_Q(state_t* buffer, void *data);

// Removes the value from the bufferQ in FIFO order and stores it in data
// Returns BUFFER_SUCCESS if the bufferQ is not empty and a value was removed
// Returns BUFFER_ERROR otherwise
enum buffer_status buffer_remove_Q(state_t* buffer, void **data);

// Writes data to the given buffer
// This is a blocking call i.e., the function only returns on a successful completion of send
// In case the buffer is full, the function waits till the buffer has space to write the new data
// Returns SUCCESS for successfully writing data to the buffer,
// CLOSED_ERROR if the buffer is closed, and
// GEN_ERROR on encountering any other generic error of any sort
enum buffer_status buffer_send(state_t* buffer, void* data);

// Reads data from the given buffer and stores it in the function’s input parameter, data (Note that it is a double pointer).
// This is a blocking call i.e., the function only returns on a successful completion of receive
// In case the buffer is empty, the function waits till the buffer has some data to read
// Returns SUCCESS for successful retrieval of data,
// CLOSED_ERROR if the buffer is closed, and
// GEN_ERROR on encountering any other generic error of any sort
enum buffer_status buffer_receive(state_t* buffer, void** data);

// Closes the buffer and informs all the blocking send/receive/select calls to return with CLOSED_ERROR
// Once the buffer is closed, send/receive/select operations will cease to function and just return CLOSED_ERROR
// Returns SUCCESS if close is successful,
// CLOSED_ERROR if the buffer is already closed, and
// GEN_ERROR in any other error case
enum buffer_status buffer_close(state_t* buffer);

// Frees all the memory allocated to the buffer
// The caller is responsible for calling buffer_close and waiting for all threads to finish their tasks before calling buffer_destroy
// Returns SUCCESS if destroy is successful,
// DESTROY_ERROR if buffer_destroy is called on an open buffer, and
// GEN_ERROR in any other error case
enum buffer_status buffer_destroy(state_t* buffer);

int get_msg_size (char* data);

#endif // BUFFER_H
