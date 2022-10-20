/**
 * @file libmythreads.c
 * @author Charles "Blue" Hartsell (ckharts@clemson.edu)
 * @brief User Mode Thread Management Library
 * @version 0.1
 * @date 2022-09-29
 * 
 * @copyright Copyright (c) 2022
 * 
 */

/* On Mac OS X, _XOPEN_SOURCE must be defined before including ucontext.h.
Otherwise, getcontext/swapcontext causes memory corruption. See:
http://lists.apple.com/archives/darwin-dev/2008/Jan/msg00229.html */
#ifdef __APPLE__
#define _XOPEN_SOURCE
#endif

#include "mythreads.h"
#include <stdbool.h>
#include <ucontext.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define MAX_NUM_THREADS 32768 // 2^15, found via 'cat /proc/sys/kernel/threads-max'

// Struct to hold thread information
typedef struct thread
{
    long id; /* thread id */
	ucontext_t context; /* Stores the current context */
	int active; /* A boolean flag, 0 if it is not active, 1 if it is */
	void* stack; /* The stack for the thread */
    void* returnValue; /* The return value of the thread */
} thread;

// condition variable struct
typedef struct condition
{
    int numWaiting; /* The number of threads waiting on the condition */
    int condNum; /* The condition number */
    bool signaled; /* A boolean flag, 0 if it is not signaled, 1 if it is */
} condition;

// lock struct
typedef struct lock
{
    bool locked; /* A boolean flag, 0 if it is not locked, 1 if it is */
    thread* owner; /* The thread that owns the lock */
    int lockNum; /* The lock number */
    condition conditions[CONDITIONS_PER_LOCK]; /* The condition variables for the lock */
} lock;

// list of locks
lock locks[NUM_LOCKS];
// list of conditions
condition conditions[MAX_NUM_THREADS];
// List of threads
static thread threadList[MAX_NUM_THREADS];

// Counter to keep track of the number of threads
static long counter;
// The main context
static ucontext_t mainContext;
// The index of the currently running thread
static int currentThread = -1;
// Boolean to indicate whether we are inside a thread
bool inThread = false;

int interruptsAreDisabled; // <- this variable is set to 1 if interrupts are disabled, 0 otherwise

/**
 * @brief Disables interrupts.
 */
static void interruptDisable () {
    //assert (! interruptsAreDisabled ) ;
    interruptsAreDisabled = 1;
}

/**
 * @brief Enables interrupts.
 */
static void interruptEnable () {
    //assert ( interruptsAreDisabled ) ;
    interruptsAreDisabled = 0;
}

/**
 * @brief Run function pointed to by funcPtr in current thread
 * 
 * @param funcPtr Pointer to function to run
 * @param argPtr Pointer to argument to pass to function
 */
static void runThread(thFuncPtr funcPtr, void *argPtr)
{
    inThread = true;

    // set the current thread to active
	threadList[currentThread].active = 1;

    // run the thread
    void* result = funcPtr(argPtr);

    // set the return value
    threadList[currentThread].returnValue = result;

    // set the thread to inactive
	threadList[currentThread].active = 0;
    inThread = false;
    
	// yield control
	threadYield();
}

// Thread Control--------------------------------------------------------

/**
 * @brief Called to initialize the thread library.
 */
extern void threadInit() {
    // initialize the thread library
    // good place to initialize data structures

    // enable interrupts
    interruptEnable();

    counter = 0; // initialize the thread id counter

    // ensure all threads are set as inactive
    for (int i = 0; i < MAX_NUM_THREADS; ++ i ) {
	    threadList[i].active = 0;
	}

    // get the main context
    getcontext(&mainContext);

    // initialize the locks
    for (int i = 0; i < NUM_LOCKS; ++ i ) {
        locks[i].locked = false;
        locks[i].owner = NULL;
        locks[i].lockNum = i;
        for (int j = 0; j < CONDITIONS_PER_LOCK; ++ j ) {
            locks[i].conditions[j].numWaiting = 0;
            locks[i].conditions[j].condNum = j;
            locks[i].conditions[j].signaled = false;
        }
    }
}

/**
 * @brief Called to create a new thread. Thread should have a stack of size STACK_SIZE.
 *  Thread should be executed by calling the function func with argument arg.
 *  If successful, return an int that uniquely identifies the new thread.
 *  The new thread should be run immediately after it is created (before threadCreate returns).
 * 
 * @param funcPtr pointer to the function to be executed by the new thread
 * @param argPtr pointer to the argument to be passed to the new thread
 * @return pid pid of the new thread
 */
extern int threadCreate(thFuncPtr funcPtr, void *argPtr) {
    interruptEnable();

    // setup the thread info
    bool threadSuccess = true;
    long id = counter; // get unique thread ID

    // create new thread with stack size STACK_SIZE
    thread newThread = {
        .id = id,
        .active = 1,
        .stack = malloc(STACK_SIZE)
    };

    // set the new thread's context
    getcontext(&newThread.context);

    // make the new thread the current thread
    currentThread = id;

    // add new thread to the list of threads
    threadList[counter] = newThread;

    // set the stack for the new thread
    newThread.context.uc_link = 0;
    newThread.context.uc_stack = (stack_t) {
        .ss_sp = threadList[counter].stack,
        .ss_size = STACK_SIZE,
        .ss_flags = 0
    };
    if (threadList[counter].stack == 0) {
        // printf("Error: Could not allocate stack for thread %ld", id);
		threadSuccess = false;
	}

    // if the new thread was successfully created, swap to it. otherwise, return -1 to indicate non-success
    if (threadSuccess == true) {
        // increment the number of threads
        ++counter;

        // make the new thread the current thread
        currentThread = id;

        // create context for new thread and execute function
        makecontext(&newThread.context, (void (*)(void)) &runThread, 2, funcPtr, argPtr);

        // swap to the new thread
        interruptDisable();
        swapcontext(&mainContext, &newThread.context);
        interruptEnable();

        return id;
    } else {
        threadList[counter] = (thread) {0};
        return -1;
    }
}

/**
 * @brief Causes currently running thread to yield the CPU.
 *  Saves the current context and selects the next thread to run.
 */
extern void threadYield() {
    interruptEnable();
    // if we are in a thread, swap to main context. otherwise run thread shutdown process
	if (inThread) {
        // find the next thread to run
        int nextThread = threadList[currentThread].id + 1;
        if(nextThread >= counter) {
            nextThread = -1;
        }

        // if there is a thread to run, swap to it
        if (nextThread != -1) {
            // set the next thread to active
            threadList[nextThread].active = 1;

            // set the current thread to the next thread
            currentThread = nextThread;

            // save current context & swap to the next thread
            interruptDisable();
            swapcontext(&threadList[currentThread].context, &threadList[nextThread].context);
            interruptEnable();
        } else {
            // if there are no threads to run, swap to the main context
            interruptDisable();
            setcontext(&mainContext);
            interruptEnable();
        }
	} else {
        // threadYield has been called from user code (force thread shutdown)

		if (counter == 0) return; // if there are no threads, return

        // check if all threads are inactive
        bool allInactive = true;
        for (int i = 0; i < counter; ++ i ) {
            if (threadList[i].active == 1) {
                allInactive = false;
                break;
            }
        }

        // find the next active thread
        int nextThread = threadList[currentThread].id + 1;
        while (threadList[nextThread].active == 0 && !allInactive) {
            nextThread = (nextThread + 1) % counter;
        }

        // set the next thread as the current thread
        currentThread = nextThread;

        // disable interrupts & swap to the next thread
        interruptDisable();
        setcontext(&threadList[nextThread].context);
        interruptEnable();
	}
}

/**
 * @brief Waits until thread corresponding to thread_id exits.
 *  If result is not NULL, then the thread function's return value is 
 *  stored at the address pointed to by 'result'. Otherwise, the thread's
 *  return value is ignored. If the thread specified by thread_id has
 *  already exited, or does not exist, then threadJoin returns immediately.
 * 
 * @param thread_id pid of the thread to wait for
 * @param result result of thread specified by thread_id
 */
extern void threadJoin(int thread_id, void **result) {
    interruptEnable();
    // ensure thread exists
    if (thread_id >= counter) {
        // printf("Error: Thread %d does not exist\n", thread_id);
        return;
    }

    // check if thread has finished
    if (threadList[thread_id].active == 0) {
        // printf("Error: Thread %d has already finished\n", thread_id);
        return;
    }

    // wait for thread to finish
    while (threadList[thread_id].active == 1) {
        threadYield();
    }

    // if return value is not null, store the thread's return value at the address pointed to by result
    if (threadList[thread_id].returnValue != NULL) {
        *result = threadList[thread_id].returnValue;
    }

    // join the thread
    threadList[thread_id].active = 0;

    // free the thread's stack
    free(threadList[thread_id].stack);
}

/**
 * @brief Causes the currently running thread to exit.
 * 
 * @param result result of the currently running thread
 */
extern void threadExit(void *result) {
    interruptEnable();
    // ensure we are in main context
    if (inThread) {
        // printf("Error: Cannot exit thread from within thread\n");
        return;
    }

    // ensure there are active threads
    bool allInactive = true;
    for (int i = 0; i < counter; ++ i ) {
        if (threadList[i].active == 1) {
            allInactive = false;
            break;
        }
    }
    if (allInactive || counter == 0) {
        // printf("Error: No active threads to exit\n");
        return;
    }

    // set the current thread's return value
    result = threadList[currentThread].returnValue;

    // free the thread's stack
    free(threadList[currentThread].stack);

    // yield control
    threadYield();
}

// Thread synchronization------------------------------------------------

/**
 * @brief Blocks (waits) until function is able to acquire the lock.
 * 
 * @param lockNum number indicating which lock is to be locked (NOT THE LOCK ITSELF)
 */
extern void threadLock(int lockNum) {
    // ensure lockNum is valid
    if (lockNum < 0 || lockNum >= NUM_LOCKS) {
        // printf("Error: Invalid lock number %d\n", lockNum);
        return;
    }

    // wait to acquire lock
    while (locks[lockNum].locked == 1) {
        threadYield();
    }

    // lock the lock
    locks[lockNum].locked = 1;
}

/**
 * @brief Unlocks the lock.
 * 
 * @param lockNum number indicating which lock is to be unlocked (NOT THE LOCK ITSELF)
 */
extern void threadUnlock(int lockNum) {
    // ensure lockNum is valid
    if (lockNum < 0 || lockNum >= NUM_LOCKS) {
        // printf("Error: Invalid lock number %d\n", lockNum);
        return;
    }

    // unlock the lock
    locks[lockNum].locked = 0;
}

/**
 * @brief Automatically unlocks lock specified by lockNum and causes current thread to
 *  block until the specified condition is signaled (by a call to threadSignal). The waiting
 *  thread unblocks only after another thread calls threadSignal with the same lock and
 *  condition variable. Mutex must be locked before calling this function, otherwise print
 *  an error message and exit.
 * 
 * @param lockNum number indicating which lock is to be unlocked (NOT THE LOCK ITSELF)
 * @param conditionNum number indicating which condition variable to wait on (NOT THE CONDITION VARIABLE ITSELF)
 */
extern void threadWait(int lockNum, int conditionNum) {
    // ensure lockNum is valid
    if (lockNum < 0 || lockNum >= NUM_LOCKS) {
        // printf("Error: Invalid lock number %d\n", lockNum);
        return;
    }

    // ensure conditionNum is valid
    if (conditionNum < 0 || conditionNum >= CONDITIONS_PER_LOCK) {
        // printf("Error: Invalid condition number %d\n", conditionNum);
        return;
    }

    // ensure lock is locked
    if (locks[lockNum].locked == 0) {
        // printf("Error: Lock %d is not locked\n", lockNum);
        return;
    }

    // unlock the lock
    locks[lockNum].locked = 0;

    // wait for the condition to be signaled
    while (locks[lockNum].conditions[conditionNum].signaled == 0) {
        threadYield();
    }

    // check for signal
    if (locks[lockNum].conditions[conditionNum].signaled == 1) {
        // reset the condition
        locks[lockNum].conditions[conditionNum].signaled = 0;

        // acquire the lock
        threadLock(lockNum);
    }

}

/**
 * @brief Unblocks a single thread waiting on the specified condition variable.
 *  If no threads are waiting on the condition variable, the function has no effect.
 * 
 * @param lockNum number indicating which lock is to be unlocked (NOT THE LOCK ITSELF)
 * @param conditionNum number indicating which condition variable to wait on (NOT THE CONDITION VARIABLE ITSELF)
 */
extern void threadSignal(int lockNum, int conditionNum) {
    // ensure lockNum is valid
    if (lockNum < 0 || lockNum >= NUM_LOCKS) {
        // printf("Error: Invalid lock number %d\n", lockNum);
        return;
    }

    // ensure conditionNum is valid
    if (conditionNum < 0 || conditionNum >= CONDITIONS_PER_LOCK) {
        // printf("Error: Invalid condition number %d\n", conditionNum);
        return;
    }

    // check if any threads are waiting on the condition
    if (locks[lockNum].conditions[conditionNum].numWaiting > 0) {
        // signal the condition
        locks[lockNum].conditions[conditionNum].signaled = 1;
    }
}
