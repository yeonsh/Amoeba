/*	@(#)Tstack.c	1.2	94/04/06 17:43:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * A simple test of the threads implementation to show that each
 * thread is running on its owm stack.
 */

#include <unistd.h>
#include "amoeba.h"
#include "thread.h"
#include "module/mutex.h"

#define STSZ	10000

mutex m_print;

factorial(n)
int n;
{
    int j;

    mu_lock(&m_print);
    printf("thread1 sp = %x\n", &j);
    mu_unlock(&m_print);
    if (n <= 1)
	return 1;
    return n * factorial(n-1);
}

thread1()
{
    int i, n;

    mu_lock(&m_print);
    printf("thread1 started\n");
    mu_unlock(&m_print);
    for (i = 1; i < 10; i++)
    {
	sleep(1);
	n = factorial(i);
	mu_lock(&m_print);
	printf("thread1 %d! = %d\n", i, n);
	mu_unlock(&m_print);
    }
}

do_stack(n)
int n;
{
    int i;
    long j;

    if (n == 1) return;
    j = (long) &i;
    mu_lock(&m_print);
    printf("thread2 j = %x\n", j);
    mu_unlock(&m_print);
    sleep(1);
    do_stack(n-1);
}

/*ARGSUSED*/
void
thread2(param, siz)
char *param;
int   siz;
{
    mu_lock(&m_print);
    printf("thread2 started\n");
    mu_unlock(&m_print);
    do_stack(10);
}

main()
{
    if (thread_newthread(thread2, STSZ, (char *) 0, 0) == 0)
	printf("couldn't start thread 2\n");
    thread1();
    thread_exit();
    /*NOTREACHED*/
}
