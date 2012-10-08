/*	@(#)thread.c	1.4	94/04/06 10:08:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
thread.c

Implementation of thread_alloc for the kernel.

Created:	Feb, 5 1992 by Philip Homburg
*/

#if KERNEL_GLOCALS

#include <assert.h>
INIT_ASSERT
#include <stdlib.h>

#include <amoeba.h>
#include <machdep.h>
#include <global.h>
#include <exception.h>
#define MPX	1
#include <kthread.h>

#if DEBUG
#define where() printf("%s, %d: ", __FILE__, __LINE__)
#endif

char *thread_alloc(index, size)
int *index;
int size;
{
	struct thread *this_thread;
	struct process *this_process;
	int ind;
	int i, tab_size, new_tab_size;
	char **table;
	char *data;

	this_thread= curthread;
	this_process= this_thread->mx_process;
	ind= *index;
	if (!ind)	/* New glocal type */
	{
		ind= *index= this_process->pr_glocals++;
		if (ind == 0)	/* First time */
			ind= *index= this_process->pr_glocals++;
	}
	assert(ind < this_process->pr_glocals);
	tab_size= this_thread->tk_mpx.MX_glocal.gtable_size;
	table= this_thread->mx_glocal.gtable;
	if (ind >= tab_size)
	{	/* Enlarge glocal table */
		new_tab_size= ind+1;
		if (!tab_size)
			table= this_thread->mx_glocal.gtable= 
				malloc(new_tab_size*sizeof(*table));
		else
			table= this_thread->mx_glocal.gtable= 
				realloc(table, new_tab_size*sizeof(*table));
		if (!table)
			panic("unable to (re)allocate glocal data table");
		for (i= tab_size; i<new_tab_size; i++)
			table[i]= NULL;
		tab_size= this_thread->mx_glocal.gtable_size= new_tab_size;
	}
	data= table[ind];
	if (!data)
	{	/* Malloc data */
		data= table[ind]= malloc(size);
	}
#if DEBUG & 256
 { where(); printf("thread= 0x%x, process= 0x%x, *index= %d, gtable= 0x%x, gtable_size= %d, data= 0x%x", this_thread, this_process, *index, this_thread->mx_glocal.gtable, this_thread->mx_glocal.gtable_size, this_thread->mx_glocal.gtable[*index]); }
#endif
	return data;
}
#else
#ifndef lint
static int nonempty;
#endif
#endif /* KERNEL_GLOCALS */
