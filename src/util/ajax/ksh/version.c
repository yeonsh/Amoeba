/*	@(#)version.c	1.3	96/02/27 13:04:44 */
/*
 * value of $KSH_VERSION
 */

#ifndef lint
static char *RCSid = "$Id: version.c,v 1.3 1994/11/14 09:06:59 versto Exp $";
#endif

#include "stdh.h"
#include <setjmp.h>
#include "sh.h"
#include "patchlevel.h"

char ksh_version [] =
	"KSH_VERSION=@(#)PD KSH v4.9 93/09/29";


