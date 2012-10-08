/*	@(#)getinfo.c	1.4	96/02/27 12:41:47 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "acv.h"
#include <stdio.h>

#if defined(NO_UNIX_AOUT) && defined(NO_ACK_OUT) && defined(NO_386_AOUT)
 #error At least one a.out format should be allowed!
#endif

#ifndef NO_UNIX_AOUT
int   isunixaout();    /* for a.out files created with the Unix host compiler */
char *unixgetinfo();
#endif

#ifndef NO_386_AOUT
int   isaout386();     /* for '386 binaries created with gcc */
char *aout386info();
#endif

#ifndef NO_ACK_AOUT
int   isackaout();     /* for ACK a.out binaries */
char *ackgetinfo();
#endif

#ifndef NO_ELF_AOUT
int   iselfaout();
char *elfgetinfo();
#endif

char *
aout_getinfo(fd, aip)
int fd;
struct aoutinfo *aip;
/* If the contents of fd match a known binary "a.out" format, the generic
 * "aoutinfo" structure is initialized using a suitable conversion routine.
 *
 * If all went well, it returns 0.
 * Otherwise it returns a string telling what the problem was.
 */
{
#ifndef NO_UNIX_AOUT
    if (isunixaout(fd)) {
	return unixgetinfo(fd, aip);
    }
#endif

#ifndef NO_386_AOUT
    if (isaout386(fd)) {
	return aout386info(fd, aip);
    }
#endif

#ifndef NO_ACK_AOUT
    if (isackaout(fd)) {
	return ackgetinfo(fd, aip);
    }
#endif

#ifndef NO_ELF_AOUT
    if (iselfaout(fd)) {
	return elfgetinfo(fd, aip);
    }
#endif

    return "unknown file type";
}
