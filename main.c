#include "mythreads.h"
#include <stdlib.h>
#include <stdio.h>

void *thread1() {
  printf("hello from thread 1\n");

  threadYield();

  printf("hello again from thread 1\n");

  return NULL;
}

void *thread2() {
  printf("hello from thread 2\n");

  threadYield();

  printf("hello again from thread 2\n");

  return NULL;
}

int main() {

    threadInit();
    
    int thread_zero = threadCreate((thFuncPtr)thread1, NULL);
    printf("threadCreate 1 #id = %d\n", thread_zero);

    int thread_one = threadCreate((thFuncPtr)thread2, NULL);
    printf("threadCreate 2 #id = %d\n", thread_one);
  
    int *result;
    threadJoin(thread_zero, (void *)&result);
    printf("joined #0 --> %d.\n", *result);
};