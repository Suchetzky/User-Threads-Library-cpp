/*
 * test3.cpp
 *
 *  Created on: Apr 8, 2015
 *      Author: roigreenberg
 *
 */

#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdlib.h>
#include <deque>
#include <list>
#include <assert.h>
#include "uthreads.h"
//#include "libuthread.a"
#include <iostream>

using namespace std;

void f(void){}

int main(void)
{
	int usecs = 10;
	if (uthread_init(usecs) == -1)
	{
        return 0;
    }


    uthread_terminate(-1);
    uthread_block(-1);
    uthread_resume(-1);
    uthread_get_quantums(-1);

    uthread_terminate(1);
    uthread_block(1);
    uthread_resume(1);
    uthread_get_quantums(1);

    uthread_block(0);

    uthread_spawn(f);
    uthread_terminate(1);

    uthread_terminate(1);
    uthread_block(1);
    uthread_resume(1);
    uthread_get_quantums(1);

    int w = -10;
	uthread_init(w);
	int m = 0;
    uthread_init(m);


    uthread_terminate(0);
    return 0;
}

