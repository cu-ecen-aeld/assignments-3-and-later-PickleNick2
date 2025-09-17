#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    struct thread_data *data = (struct thread_data*) thread_param;

    // Wait
    sleep(data->wait_to_obtain_ms/1000);

    // Obtain mutex
    int rc1 = pthread_mutex_lock(data->mutex);
    if(rc1 != 0){
        ERROR_LOG("Failed to lock mutex");
        data->thread_complete_success = false;
        return data;
    }
    //  Wait
    sleep(data->wait_to_release_ms/1000);

    // Release mutex
    int rc2 = pthread_mutex_unlock(data->mutex);
    if(rc2 != 0){
        ERROR_LOG("Failed to unlock mutex");
        data->thread_complete_success = false;
        return data;
    }

    data->thread_complete_success = true;
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */


    //Allocate memory for the thread_data struct
    struct thread_data *data = malloc(sizeof(struct thread_data));
    if(data == NULL){
        ERROR_LOG("Could not allocate memory for thread_data");
        return false;
    }
// Define all the parameters for the struct to be used in this function
    data->mutex = mutex;
    data->thread_complete_success = false;
    data->wait_to_obtain_ms = wait_to_obtain_ms;
    data->wait_to_release_ms = wait_to_release_ms;
// Create the thread
    int rc = pthread_create(thread, NULL, threadfunc, data);
    if(rc != 0){
        ERROR_LOG("pthread_create failed");
        free(data);
        return false;
    }





    return true;
}

