/*
 * Simple hello world test
 *
 * Tests the creation of a single thread and its successful return.
 */

#include <stdio.h>
#include <stdlib.h>

#include "mythreads.h"

int interruptsAreDisabled = 0;

void hello(void* arg)
{
	printf("Hello\n");
}

int main(void)
{
    threadInit();

    void* vp;
    thFuncPtr fp;
    fp = (thFuncPtr) &hello;
    int id = threadCreate(fp, vp);

    threadYield();

    id = threadCreate(fp, vp);

    void **result;
    threadJoin(id, result);
	return 0;
}