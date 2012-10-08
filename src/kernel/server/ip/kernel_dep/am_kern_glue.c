/*	@(#)am_kern_glue.c	1.3	96/02/27 14:19:56 */
/*
am_kernel_glue.c

Created Sept 27, 1991 by Philip Homburg
*/

#include "inet.h"
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
	char buffer[64], *p;

	p= bprintf(buffer, buffer+sizeof(buffer)-1, "%s/%s", TCPIP_DIRNAME,
		name);
	*p= '\0';
	dirappend(buffer, cap);
}
