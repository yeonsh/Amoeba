/*	@(#)common.c	1.4	96/02/27 13:11:43 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * common.c
 *
 *	Code shared by 'at' and 'cronsubmit'
 *
 * Author: Greg Sharp, Aug 1991 - derived from the common code in
 *				  Maarten Huisjes' code for at and cronsubmit
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "caplist.h"
#include "ampolicy.h"
#include "module/name.h"

#include "stdlib.h"
#include "string.h"
#include "stdio.h"

#include "common.h"
#include "sak.h"

extern char *		progname;
extern struct caplist	capsenv[];
extern char *		strenv[];

extern void	usage _ARGS((void));


char *
memalloc(bytes)
size_t bytes;
{
    char *	p;

    if ((p = (char *) malloc(bytes)) == (char *) 0)
    {
	fprintf(stderr, "%s: Couldn't malloc %u bytes\n", progname, bytes);
	exit(-1);
	/* NOTREACHED */
    }
    return p;
}


void
addstrenv(name, ptr)
char *	name;
char *	ptr;
{
    int		i;
    char *	p;

    for (i = 0; strenv[i] != (char *) 0; i++)
    {
	p = strchr(strenv[i], '=');
	if (p == (char *) 0) /* not possible */
	    continue;
	
	if (strncmp(name, strenv[i], (size_t) (p - strenv [i])) == 0)
	{
	    free((_VOIDSTAR) strenv[i]);
	    break;
	}
    }
    if (i >= MAX_ENVS)
    {
	fprintf(stderr, "%s: too many string environment strings\n", progname);
	exit(-1);
    }

    strenv[i] = memalloc(strlen(name) + strlen(ptr) + 2);
    (void) sprintf(strenv[i], "%s=%s", name, ptr);

    return;
}


void
add2strenv(arg)
char *arg;
{
    char *	p;

    if ((p = strchr(arg, '=')) == (char *) 0)
    {
	fprintf(stderr, "%s: bad -E flag specification\n", progname);
	usage();
	/* NOTREACHED */
    }
    /* temporarily replace '=' by '\0' to split it into <name,value> */
    *p = '\0';
    addstrenv(arg, p + 1);
    *p = '='; /* restore */
}


void
addcapenv(name, pcap)
char *		name;
capability *	pcap;
{
    int	i;

    for (i = 0; capsenv[i].cl_name != (char *) 0; i++)
    {
	if (strcmp(capsenv[i].cl_name, name) == 0)
	{
	    *capsenv[i].cl_cap = *pcap;
	    return;
	}
    }
    if (i >= MAX_CAPS)
    {
	fprintf(stderr, "%s: too many capability environment strings\n",
								    progname);
	exit(-1);
	/*NOTREACHED*/
    }

    capsenv[i].cl_name = memalloc(strlen(name) + 1);
    (void) strcpy(capsenv[i].cl_name, name);
    capsenv[i].cl_cap = (capability *) memalloc(sizeof (capability));
    *capsenv[i].cl_cap = *pcap;
}


void
add2capenv(arg)
char *arg;
{
    char *	p;
    capability	cap;
    errstat	err;

    if ((p = strchr(arg, '=')) == (char *) 0)
    {
	fprintf(stderr, "%s: bad -C flag specification\n", progname);
	usage ();
	/* NOTREACHED */
    }
    *p++ = '\0';

    if ((err = name_lookup(p, &cap)) != STD_OK)
    {
	fprintf(stderr, "%s: can't get capability for %s: %s\n",
						progname, p, err_why(err));
	exit(-1);
	/* NOTREACHED */
    }

    addcapenv(arg, &cap);
}


void
empty_strenv()
{
    int	i;

    for (i = 0; strenv[i] != (char *) 0; i++)
    {
	free((_VOIDSTAR) strenv[i]);
	strenv[i] = (char *) 0;
    }

    addstrenv("HOME",  DEFAULT_HOME); /* see ampolicy.h */
    addstrenv("WORK",  DEF_WORK);
    addstrenv("SHELL", DEF_SHELL);
}
