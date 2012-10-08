/*	@(#)difftime.c	1.1	91/04/09 08:44:56 */
#ifndef lint
#ifndef NOID
static char	elsieid[] = "@ (#)difftime.c	7.1";
#endif /* !defined NOID */
#endif /* !defined lint */

/*LINTLIBRARY*/

#include "private.h"

#undef difftime

double
difftime(time1, time0)
const time_t	time1;
const time_t	time0;
{
	return time1 - time0;
}
