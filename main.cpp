#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/time.h>
#include <string>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include "types_p3.h"
#include "p3_threads.h"
#include "utils.h"
#include <climits>

// pthread conditional variable to start/resume the thread
pthread_cond_t resume[4];

// pthread conditional variable to wait for threads to finish init
pthread_cond_t init[4];

// pthread conditional variable to signal when the thread's task is done
pthread_cond_t a_task_is_done;

// pthread conditional variable to allow the scheduler to wait for a thread to preempt
pthread_cond_t preempt_task;

// tcbs for each thread
ThreadCtrlBlk tcb[4];

// ready queue of threads
std::vector<int> ready_queue;

// number of tasks that did not miss deadline
int num_of_alive_tasks = 4;

// -1 = no thread working, <number> = thread <number> currently working
int running_thread = -1;

// 0 = don't preempt, 1 = preempt current running thread
int preempt = 0;

// mutex used to protect variables defined in this file
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// mutex used for the task done pthread conditional variable
pthread_mutex_t taskDoneMutex = PTHREAD_MUTEX_INITIALIZER;

// marks the "start time"
struct timeval t_global_start;
// Declaration of edf_quickSort
void edf_Sort(std::vector<int>& v, int low, int high);
void swap(int* a, int* b);

// used to tell threads when to stop working (after 240 iterations)
int global_work = 0;

void fifo_schedule(void);
void edf_schedule(void);
void rm_schedule(void);

int main(int argc, char** argv)
{
	if(argc !=2 || atoi(argv[1]) < 0 || atoi(argv[1]) > 2)
	{
		std::cout << "[ERROR] Expecting 1 argument, but got " << argc-1 << std::endl;
		std::cout<< "[USAGE] p3_exec <0, 1, or 2>" << std::endl;
		return 0;
	}
	int schedule = atoi(argv[1]);

	// pthreads we are creating
	pthread_t tid[4];

	// This is to set the global start time
	gettimeofday(&t_global_start, NULL);

	// initialize all tcbs
	tcb[0].id = 0;
	tcb[0].task_time = 200;
	tcb[0].period = 1000;
	tcb[0].deadline = 1000;

	tcb[1].id = 1;
	tcb[1].task_time = 500;
	tcb[1].period = 2000;
	tcb[1].deadline = 2000;

	tcb[2].id = 2;
	tcb[2].task_time = 1000;
	tcb[2].period = 4000;
	tcb[2].deadline = 4000;

	tcb[3].id = 3;
	tcb[3].task_time = 1000;
	tcb[3].period = 6000;
	tcb[3].deadline = 6000;

	// initialize all pthread conditional variables
	for (int i = 0; i < 4; i++) {
		pthread_cond_init(&resume[i], NULL);
		pthread_cond_init(&init[i], NULL);
	}
	pthread_cond_init(&a_task_is_done, NULL);
	pthread_cond_init(&preempt_task, NULL);

	// allow all threads to work
	global_work = 1;
	printf("[Main] Create worker threads\n");

	// create pthreads and pass their respective tcb as a parameter
	pthread_mutex_lock(&mutex);
	for (int i = 0; i < 4; i++) {
		if(pthread_create(&tid[i], NULL, threadfunc, &tcb[i])) {
			fprintf(stderr, "Error creating thread\n");
		}
		// Wait until the threads are "ready" and in the ready queue
		pthread_cond_wait(&init[i], &mutex);
	}
	pthread_mutex_unlock(&mutex);

	// Reset the global time and skip the initial wait
	gettimeofday(&t_global_start, NULL);

	for (int i = 0; i < 240; i++) {
		// Select scheduler based on argv[1]
		switch(schedule) {
			case 0:
				fifo_schedule();
				break;
			case 1:
				edf_schedule();
				break;
			case 2:
				rm_schedule();
				break;
		}

		// Wait until the next 100ms interval or until a task is done
		int sleep = 100 - (get_time_stamp() % 100);
		if (num_of_alive_tasks > 0) {
			timed_wait_for_task_complition(sleep);
		} else {
			printf("All the tasks missed the deadline\n");
			break;
		}
	}

	// after 240 iterations, finish off all threads
	printf("[Main] It's time to finish the threads\n");

	printf("[Main] Locks\n");
	pthread_mutex_lock(&mutex);
	global_work = 0;

	// signal all the processes in the ready queue so they finish
	usleep(MSEC(3000));
	while (ready_queue.size() > 0) {
		pthread_cond_signal(&resume[ready_queue[0]]);
		ready_queue.erase(ready_queue.begin());
	}

	printf("[Main] Unlocks\n");
	pthread_mutex_unlock(&mutex);

	/* wait for the threads to finish */
	for (int i = 0; i < 4; i++) {
		if(pthread_join(tid[i], NULL)) {
			fprintf(stderr, "Error joining thread\n");
		}
	}

	return 0;
}


//fifo 0 -- first come for serve behaviour

void fifo_schedule(void)
{
    pthread_mutex_lock(&mutex);
		
	//check fo tasks in ready queue	
	if (!ready_queue.empty()) { 
        
		// Check if no thread is currently running
		if (running_thread == -1) {
				pthread_cond_signal(&(resume[ready_queue[0]]));	
				ready_queue.erase(ready_queue.begin()+0);	
        }
    }
    pthread_mutex_unlock(&mutex); 
}


//edf 1 -- work on the earliest deadline first
void edf_schedule(void)
{
    if (ready_queue.size() > 0) {
        pthread_mutex_lock(&mutex);
        edf_Sort(ready_queue, 0, ready_queue.size() - 1);
        pthread_mutex_unlock(&mutex);

        // Check if no thread is currently running or if preemption is needed
        if (running_thread == -1 || tcb[ready_queue[0]].deadline < tcb[running_thread].deadline) {
            pthread_mutex_lock(&mutex);
            // If a thread is currently running and it has a later deadline, set preemption flag
            if (running_thread != -1 && tcb[ready_queue[0]].deadline < tcb[running_thread].deadline) {
                preempt = 1;
                // Signal the running thread to preempt
                pthread_cond_signal(&preempt_task);
            } else {
                // If no thread is running or the earliest deadline thread should be scheduled immediately
                pthread_cond_signal(&(resume[ready_queue[0]]));
                ready_queue.erase(ready_queue.begin() + 0);
            }
            pthread_mutex_unlock(&mutex);
        }
    }
}


//merge sort for edf based on the deadline times
void merge(std::vector<int>& v, int low, int mid, int high) {
    int leftSize = mid - low + 1;
    int rightSize = high - mid;

    std::vector<int> left(leftSize);
    std::vector<int> right(rightSize);

    // Copy data to temp arrays left[] and right[]
    for (int i = 0; i < leftSize; ++i) {
        left[i] = v[low + i];
    }
    for (int j = 0; j < rightSize; ++j) {
        right[j] = v[mid + 1 + j];
    }

    // Merge the temp arrays back into v[low to high]
    int i = 0, j = 0, k = low;
    while (i < leftSize && j < rightSize) {
        if (tcb[left[i]].deadline <= tcb[right[j]].deadline) {
            v[k++] = left[i++];
        } else {
            v[k++] = right[j++];
        }
    }

    // Copy the remaining elements of left[], if any
    while (i < leftSize) {
        v[k++] = left[i++];
    }

    // Copy the remaining elements of right[], if any
    while (j < rightSize) {
        v[k++] = right[j++];
    }
}


void mergeSort(std::vector<int>& v, int low, int high) {
    if (low < high) {
        int mid = low + (high - low) / 2;

        // left half
        mergeSort(v, low, mid);

		// right half
        mergeSort(v, mid + 1, high);

        // Merge both sorted halves
        merge(v, low, mid, high);
    }
}

void edf_Sort(std::vector<int>& v, int low, int high) {
    mergeSort(v, low, high);
}


void merge_rm(std::vector<int>& v, int low, int mid, int high) {
    int leftSize = mid - low + 1;
    int rightSize = high - mid;

    std::vector<int> left(leftSize);
    std::vector<int> right(rightSize);

    // Copy data to temp arrays left[] and right[]
    for (int i = 0; i < leftSize; ++i) {
        left[i] = v[low + i];
    }
    for (int j = 0; j < rightSize; ++j) {
        right[j] = v[mid + 1 + j];
    }

    // Merge the temp arrays back into v[low to high]
    int i = 0, j = 0, k = low;
    while (i < leftSize && j < rightSize) {
        if (tcb[left[i]].period <= tcb[right[j]].period) {
            v[k++] = left[i++];
        } else {
            v[k++] = right[j++];
        }
    }

    // Copy the remaining elements of left[], if any
    while (i < leftSize) {
        v[k++] = left[i++];
    }

    // Copy the remaining elements of right[], if any
    while (j < rightSize) {
        v[k++] = right[j++];
    }
}


void mergeSort_RM(std::vector<int>& v, int low, int high) {
    if (low < high) {
        int mid = low + (high - low) / 2;

        // left half
        mergeSort(v, low, mid);

		// right half
        mergeSort(v, mid + 1, high);

        // Merge both sorted halves
        merge(v, low, mid, high);
    }
}



/// global flag to check if we need preemption for thread priority
bool preempt_needed = false;

//rm -- 2 : complete by the periods for each thread
void rm_schedule() {
    if (ready_queue.size() > 0) {

        pthread_mutex_lock(&mutex);
		//sort the threads by period
        mergeSort_RM(ready_queue, 0, ready_queue.size() - 1);
        pthread_mutex_unlock(&mutex);

        // Check if no thread is currently running or if preemption is needed
        if (running_thread == -1 || tcb[ready_queue[0]].period < tcb[running_thread].period) {
            pthread_mutex_lock(&mutex);
            // If a thread is currently running and it has a longer period, set preemption flag
            if (running_thread != -1) {
                preempt = 1;
                // Signal the running thread to preempt
                pthread_cond_signal(&preempt_task);
            } else {
                // If no thread is running or the shortest period thread should be scheduled immediately
                pthread_cond_signal(&(resume[ready_queue[0]]));
                ready_queue.erase(ready_queue.begin() + 0);
            }
            pthread_mutex_unlock(&mutex);
        }
    }
}
