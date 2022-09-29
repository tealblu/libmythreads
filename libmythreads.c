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

#include "mythreads.h"

/**
 * @brief Called to initialize the thread library.
 */
extern void threadInit() {
    // function body
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
    int pid; // <- generate a unique pid for the new thread here
    // function body

    return pid;
}

/**
 * @brief Causes currently running thread to yield the CPU.
 *  Saves the current context and selects the next thread to run.
 */
extern void threadYield() {
    // function body
}

/**
 * @brief Waits until thread corresponding to thread_id exits.
 *  If result is not NULL, then the thread function's return value is 
 *  stored at the address pointed to by result. Otherwise, the thread's
 *  return value is ignored. If the thread specified by thread_id has
 *  already exited, or does not exist, then threadJoin returns immediately.
 * 
 * @param thread_id pid of the thread to wait for
 * @param result result of thread specified by thread_id
 */
extern void threadJoin(int thread_id, void **result) {
    // function body
}

/**
 * @brief Causes the currently running thread to exit.
 * 
 * @param result result of the currently running thread
 */
extern void threadExit(void *result) {
    // function body
}

/**
 * @brief Blocks (waits) until function is able to acquire the lock.
 * 
 * @param lockNum number indicating which lock is to be locked (NOT THE LOCK ITSELF)
 */
extern void threadLock(int lockNum) {
    // function body
}

/**
 * @brief Unlocks the lock.
 * 
 * @param lockNum number indicating which lock is to be unlocked (NOT THE LOCK ITSELF)
 */
extern void threadUnlock(int lockNum) {
    // function body
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
    // function body
}

/**
 * @brief Unblocks a single thread waiting on the specified condition variable.
 *  If no threads are waiting on the condition variable, the function has no effect.
 * 
 * @param lockNum number indicating which lock is to be unlocked (NOT THE LOCK ITSELF)
 * @param conditionNum number indicating which condition variable to wait on (NOT THE CONDITION VARIABLE ITSELF)
 */
extern void threadSignal(int lockNum, int conditionNum) {
    // function body
}

extern int interruptsAreDisabled; // <- this variable is set to 1 if interrupts are disabled, 0 otherwise