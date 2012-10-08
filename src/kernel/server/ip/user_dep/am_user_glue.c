/*	@(#)am_user_glue.c	1.1	91/11/25 13:15:08 */
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
	int result;

	result= thread_newthread( (void (*) _ARGS(( char *param, int psize )))
		func, stacksiz, NULL, 0);
	if (!result)
		ip_panic(( "thread_newthread failed" ));
}

void append_name(name, cap)
char *name;
capability *cap;
{
	errstat error;

        (void) name_delete(name);
	error= name_append(name, cap);
	if (error != STD_OK)
	{
		ip_panic(( "unable to append '%s': %s", name,
				err_why(error) ));
	}
}
