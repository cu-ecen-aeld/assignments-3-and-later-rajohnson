#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <poll.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

	// delay until it is time to take the mutex	
	poll(0, 0, thread_func_args->pre_mutex_delay_ms);
	
	// take mutex
	int result = pthread_mutex_lock(thread_func_args->mutex);
	if(result != 0) {
		thread_func_args->thread_complete_success = false;
		return thread_param;
	}

	// delay until time to hold mutex passes
	poll(0, 0, thread_func_args->pre_mutex_delay_ms);

	// release mutex
	result = pthread_mutex_unlock(thread_func_args->mutex);
	if(result != 0) {
		thread_func_args->thread_complete_success = false; 
		return thread_param;
	}

   	thread_func_args->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
	// allocate memory
	struct thread_data* td = malloc(sizeof(struct thread_data));
	if(td == NULL) {
		return false;
	}

	// fill data structure
	td->pre_mutex_delay_ms = wait_to_obtain_ms;
	td->mutex_hold_delay_ms = wait_to_release_ms;
	td->mutex = mutex;

	int result = pthread_create(thread, NULL, threadfunc, (void*)td);	

    return result == 0;
}

