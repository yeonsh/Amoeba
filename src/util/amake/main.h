/*	@(#)main.h	1.4	96/02/27 13:06:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* flags */
EXTERN int
    F_no_exec,			/* don't really execute commands */
    F_cleanup,			/* throw away all produced objects & admin */
    F_no_warnings,		/* don't warn */
    F_keep_intermediates,	/* don't remove produced objects from workdir*/
    F_verbose,			/* be verbose */
    F_silent,			/* don't echo commands */
    F_one_version,		/* only keep objects used in current run */
    F_globalize,		/* experimental: optimise statefile */
    F_link_opt,			/* try to avoid lots of [un]link() calls */
    F_use_rsh;			/* use rsh to start up commands remotely */


EXTERN int Verbosity; /* the higher the more verbose */
    
EXTERN int F_reading;
/* F_reading will be set to FALSE when we have finished reading the Amakefile
 * and Statefile.
 */

#ifndef AMAKEFILE
# define AMAKEFILE   "Amakefile"
#endif

EXTERN char *Amakefile;			      /* changeable via -f */

/* search list can be extended via -L <libpath>, or assignment to variable
 * AMAKELIB in the description file. AMAKELIBDIR is tried as last resort.
 */
#ifndef AMAKELIBDIR
#  ifdef AMOEBA
#    define AMAKELIBDIR "/usr/lib/amake"
#  else
#    define AMAKELIBDIR "/usr/local/lib/amake"
#  endif
#endif

#ifdef __STDC__
void StructReport	P((char *name, int size, int number, void *free_list));
#else
void StructReport	P((char *name , int size , int number ,
			   struct slist *free_list ));
#endif
void InitDummies	P((void));

