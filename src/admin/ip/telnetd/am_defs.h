/*	@(#)am_defs.h	1.2	96/02/27 10:15:31 */
#ifndef _AM_DEFS_H_
#define _AM_DEFS_H_
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>

typedef struct {
    capability       proccap;        /* process capability           */
    struct circbuf  *tty_inq;       /* tty server input queue       */
    struct circbuf  *tty_outq;      /* tty server output queue      */
    int		     max_col;
    int		     max_row;
} TScreen;

extern struct Term {
    TScreen screen;
} *term;

/* prototypes of the functions declared in the Amoeba-specific modules: */

int syslog _ARGS((int, char *, ...));
int openlog _ARGS((char *, int, int));
int shutdown _ARGS((int, int));

void SomethingHappened _ARGS((void));
void WaitForSomething  _ARGS((void));

void tty_init _ARGS((void));
void tty_change_window_size _ARGS((int, int));

void FatalError _ARGS((char *format, ...));
void warning _ARGS((char *format, ...));

#define bcopy(a,b,c)    memmove(b,a,c)
#define bzero(a,b)      memset(a,0,b)
#define bcmp(a,b,c)     memcmp(a,b,c)

#endif /* am_defs.h */
