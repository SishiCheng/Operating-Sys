#include "buffer.h"

// adds data to buffer if available size in Q is greater than data size  and returns BUFFER_SUCCESS
// otherwise returns BUFFER_ERROR 

enum buffer_status buffer_add_Q(state_t* buffer, void* data)
{
    int size_data = strlen(data)+1;
    if(buffer->fifoQ->avilSize > (sizeof(int) + size_data ))
    {
        fifo_write(buffer->fifoQ, &size_data, sizeof(int));
        fifo_write(buffer->fifoQ, data, size_data);
        return BUFFER_SUCCESS;
    }
    return BUFFER_ERROR;    
}

// removes data from buffer (only if there is any)  and returns BUFFER_SUCCESS

enum buffer_status buffer_remove_Q(state_t* buffer, void **data)
{
    if (buffer->fifoQ->avilSize < buffer->fifoQ->size) {
        int actual_data_size ;
        fifo_read(buffer->fifoQ, &actual_data_size, sizeof(int) );
        fifo_read(buffer->fifoQ, (*data), actual_data_size );
        return BUFFER_SUCCESS;
    }
    return BUFFER_ERROR;
}

//finds data pressent at "index" location in Q
enum buffer_status buffer_top_message (state_t* buffer, void **data,int index)
{
    int actual_data_size ;
    int TAIL = buffer->fifoQ->tail;
    int HEAD = buffer->fifoQ->head;

    int move_bytes = 0;
    for(int i=0;i<index;i++)
    {
        move_bytes += fifo_read(buffer->fifoQ, &actual_data_size, sizeof(int) );
        move_bytes += fifo_read(buffer->fifoQ, (*data), actual_data_size );
    }


    buffer->fifoQ->avilSize-=(move_bytes) ;
    buffer->fifoQ->tail = TAIL;
    buffer->fifoQ->head = HEAD;
    return BUFFER_SUCCESS;
}

// returns the size of one message 
// a message is combination of one int and actual string data
// actual string data is preceded by an integer value having "actual data's" size

int get_msg_size (char* data)
{
    return ( sizeof(int) + strlen(data)+1); 
}