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
#include <malloc.h>
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
    int returnValue; /* The return value of the thread */
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

// Counter to keep track of the number of threads
static long counter;
// The main context
static ucontext_t mainContext;
// List of threads
static thread threadList[MAX_NUM_THREADS];
// The number of extant threads
static int numThreads;
// The index of the currently running thread
static int currentThread = -1;
// Boolean to indicate whether we are inside a thread
bool inThread = false;

/**
 * @brief Get the next thread id
 * 
 * @return counter incremented by 1
 */
int getThreadID() {
    return counter++;
}

/**
 * @brief Run function pointed to by funcPtr in current thread
 * 
 * @param funcPtr Pointer to function to run
 * @param argPtr Pointer to argument to pass to function
 */
static void runThread(thFuncPtr funcPtr, void *argPtr)
{
    // set the current thread to active
	threadList[currentThread].active = 1;

    // run the thread
	funcPtr(argPtr);

    // set the thread to inactive
	threadList[currentThread].active = 0;
	
	// yield control
	threadYield();
}

// Thread Control Block--------------------------------------------------------

/**
 * @brief Called to initialize the thread library.
 */
extern void threadInit() {
    // initialize the thread library
    // good place to initialize data structures

    counter = 0; // initialize the thread id counter
    numThreads = 0; // initialize the number of threads

    // ensure all threads are set as inactive
    for (int i = 0; i < MAX_NUM_THREADS; ++ i ) {
	    threadList[i].active = 0;
	}

    // get the main context
    getcontext(&mainContext);
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
    // setup the thread info
    bool threadSuccess = true;
    long id = getThreadID(); // get unique thread ID
    ucontext_t currentContext;
    getcontext(&currentContext); // get the current context

    // create new thread with stack size STACK_SIZE
    thread newThread = {
        .id = id,
        .context = currentContext,
        .active = 1,
        .stack = malloc(STACK_SIZE)
    };

    // set the stack for the new thread
    newThread.context.uc_stack.ss_sp = threadList[numThreads].stack;
	newThread.context.uc_stack.ss_size = STACK_SIZE;
	newThread.context.uc_stack.ss_flags = 0;

    // make the new thread the current thread
    currentThread = id;

    // make the new thread's context a child of the current thread
    newThread.context.uc_link = &threadList[currentThread].context;

    // add new thread to the list of threads
    threadList[numThreads] = newThread;

    if (threadList[numThreads].stack == 0) {
        printf("Error: Could not allocate stack for thread %ld", id);
		threadSuccess = false;
	}

    // increment the number of threads
    ++numThreads;

    // create context for new thread and execute function
    makecontext(&newThread.context, (void (*)(void)) &runThread, 2, funcPtr, argPtr);

    // if the new thread was successfully created, swap to it. otherwise, return -1 to indicate non-success
    if (threadSuccess) {
        swapcontext(&currentContext, &newThread.context);
        return id;
    } else {
        return -1;
    }
}

/**
 * @brief Causes currently running thread to yield the CPU.
 *  Saves the current context and selects the next thread to run.
 */
extern void threadYield() {
    // if we are in a thread, swap to main context. otherwise run thread shutdown process
	if (inThread) {
        swapcontext(&threadList[currentThread].context, &mainContext);
        
	} else {
		if (numThreads == 0) return; // if there are no threads, return
	
		// get the next thread to run
		currentThread = (currentThread + 1) % numThreads;
		
		// call the next thread
		inThread = 1;
		swapcontext( &mainContext, &threadList[currentThread].context );
		inThread = 0;
		
        // cleanup the thread once it finishes
		if (threadList[currentThread].active == 0) {
			// Free the thread's stack
			free(threadList[currentThread].stack );
			
			// Swap the last thread with the current thread
			--numThreads;
			if (currentThread != numThreads)
			{
				threadList[currentThread] = threadList[numThreads];
			}
			threadList[numThreads].active = 0;		
		}
		
	}
	return;
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
    // check for thread joining itself
    if (thread_id == currentThread) {
        printf("Error: Thread %d cannot join itself", thread_id);
        return;
    }

    // ensure thread is created
    if (threadList[thread_id].active == 0) {
        printf("Error: Thread %d does not exist", thread_id);
        return;
    }

    // wait for thread to finish
    while (threadList[thread_id].active == 1) {
        threadYield();
    }

    // if result is not null, store the thread's return value at the address pointed to by result
    if (result != NULL) {
        *result = threadList[thread_id].returnValue;
    }
}

/**
 * @brief Causes the currently running thread to exit.
 * 
 * @param result result of the currently running thread
 */
extern void threadExit(void *result) {
    // ensure we are in main context
    if (inThread) {
        printf("Error: Cannot exit thread from within thread");
        return;
    }

    // set the current thread's return value
    threadList[currentThread].returnValue = result;

    // yield control
    threadYield();
}

// Thread synchronization block------------------------------------------------

/**
 * @brief Blocks (waits) until function is able to acquire the lock.
 * 
 * @param lockNum number indicating which lock is to be locked (NOT THE LOCK ITSELF)
 */
extern void threadLock(int lockNum) {
    // ensure lockNum is valid
    if (lockNum < 0 || lockNum >= NUM_LOCKS) {
        printf("Error: Invalid lock number %d", lockNum);
        return;
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
        printf("Error: Invalid lock number %d", lockNum);
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
        printf("Error: Invalid lock number %d", lockNum);
        return;
    }

    // ensure conditionNum is valid
    if (conditionNum < 0 || conditionNum >= CONDITIONS_PER_LOCK) {
        printf("Error: Invalid condition number %d", conditionNum);
        return;
    }

    // ensure lock is locked
    if (locks[lockNum].locked == 0) {
        printf("Error: Lock %d is not locked", lockNum);
        return;
    }

    // unlock the lock
    locks[lockNum].locked = 0;

    // wait for the condition to be signaled
    while (locks[lockNum].conditions[conditionNum].signaled == 0) {
        threadYield();
    }

    // lock the lock
    locks[lockNum].locked = 1;

    // reset the condition
    conditions[conditionNum].signaled = 0;        
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
        printf("Error: Invalid lock number %d", lockNum);
        return;
    }

    // ensure conditionNum is valid
    if (conditionNum < 0 || conditionNum >= CONDITIONS_PER_LOCK) {
        printf("Error: Invalid condition number %d", conditionNum);
        return;
    }

    // signal the condition
    locks[lockNum].conditions[conditionNum].signaled = 1;
}

extern int interruptsAreDisabled; // <- this variable is set to 1 if interrupts are disabled, 0 otherwise

/**
 * @brief Disables interrupts.
 */
static void interruptDisable () {
    assert (! interruptsAreDisabled ) ;
    interruptsAreDisabled = 1;
}

/**
 * @brief Enables interrupts.
 */
static void interruptEnable () {
    assert ( interruptsAreDisabled ) ;
    interruptsAreDisabled = 0;
}
