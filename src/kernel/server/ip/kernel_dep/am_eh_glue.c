/*	@(#)am_eh_glue.c	1.2	92/06/25 15:18:40 */
/*
am_kernel_glue.c

Created Sept 27, 1991 by Philip Homburg
*/

#include "nw_task.h"
#include "amoeba_incl.h"

void new_thread(func, stacksiz)
void (*func) _ARGS(( void ));
size_t stacksiz;
{
	NewKernelThread(func, stacksiz);
}

void append_name(name, cap)
char *name;
capability *cap;
{
	dirappend(name, cap);
}
