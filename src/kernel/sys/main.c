/*	@(#)main.c	1.9	96/02/27 14:22:13 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "printbuf.h"
#include "table.h"
#include "map.h"
#include "assert.h"
INIT_ASSERT
#include "proto.h"
#include "version.h"

#ifdef PB_START
int pb_busy; /* Nonzero if someone is waiting for a printbuf event. */
#endif

#ifndef NO_OUTPUT_ON_CONSOLE

#include "tty/term.h"

int console_disabled;	/* True if not outputting on the console */

console_enable(enable)
    int enable;
{
    console_disabled = !enable;
}

#ifdef SWITCH_CONSOLE
int
console_switch(begin, end)
char *	begin;
char *	end;
{
    /* Make console output switchable via kstat or control-key. */
    char *p;

    console_disabled = !console_disabled;
    p = bprintf(begin, end, "console %s\n",
		console_disabled ? "disabled" : "enabled");
    return p - begin;
}

void
console_off()
{
    console_enable(0);
}
#endif

#endif /* NO_OUTPUT_ON_CONSOLE */

#ifndef SMALL_KERNEL
int
uptimedump(begin, end)
char *	begin;
char *	end;
{
    char *		p;

    /*
    ** This message *must* begin with the uptime in decimal seconds,
    ** since it will be parsed by ppload.
    */
    p = bprintf(begin, end, "%lu seconds up\n", getmilli() / 1000);
    return p - begin;
}
#endif

putchr(c)		/* print a character.  Do newline processing */
char c;
{
#ifdef PB_START
   if (pb_descr)
   {
	if (pb_descr->pb_next < pb_descr->pb_buf ||
	    pb_descr->pb_next >= pb_descr->pb_end)
		pb_descr->pb_next = pb_descr->pb_buf;
	*pb_descr->pb_next= c;
	pb_descr->pb_next++;
	if (pb_descr->pb_next == pb_descr->pb_end)
		pb_descr->pb_next = pb_descr->pb_buf;
	if (pb_descr->pb_next == pb_descr->pb_first)
	{
		pb_descr->pb_first++;
		if (pb_descr->pb_first == pb_descr->pb_end)
			pb_descr->pb_first = pb_descr->pb_buf;
	}
	if (pb_busy) {
		/* Only do this when a printbuf server request is pending.
		 * Otherwise we would be going all through the thread table
		 * for nothing.
		 */
		wakeup((event) pb_descr);
	}
   }
#endif

#ifndef NO_OUTPUT_ON_CONSOLE
  if (console_disabled) return;
  if (c == '\n')
	(void) putchar((struct io *) 0, '\r');
  (void) putchar((struct io *) 0, c);
#endif
}

clearbss(){
  extern char	end;
  register char *p = (char *) &end;
#if (defined(__GNUC__) && defined(__sparc__)) || defined(__SUNPRO_C)
  /* 'begin' isn't guaranteed to be at the beginning of bss, so we'll
   * use edata instead.  Note that this is really linker dependent.
   */
  extern int	edata;
  register int cnt = p - (char *)&edata;
#else
  extern int	begin;
  register int cnt = p - (char *)&begin;
#endif

  disable();			/* disable interrupts */
  while (--cnt >= 0)
      *--p = 0;		/* clear bss */
}

int
versiondump(begin, end)
char *	begin;
char *	end;
{
    extern char version[];
    char *	p;

    p = bprintf(begin, end, "%s #%s\n", AMOEBA_VERSION, version);
    return p - begin;
}

void
helloworld() {

  (void) versiondump((char *) 0, (char *) 0); /* dump to console */
}

void
exitkernel() {
#ifdef KGDB
	/*
	 * We call the debugger here instead of from abort since
	 * we don't want to stop the drivers (the debugger might
	 * restart the kernel, or call some functions).
	 */
	extern int kgdb_enabled;
	if (kgdb_enabled) breakpoint();
#endif

	stop_kernel();
	abort();
}

#ifdef NO_INPUT_FROM_CONSOLE
/*ARGSUSED*/
void
readint(c)
long c;
{
#ifndef SMALL_KERNEL
    extern struct dumptab dumptab[];
    register struct dumptab *d;

    for(d=dumptab; d->d_func; d++) {
	if (c == d->d_char) {
	    (*d->d_func)((char *)0, (char *)0);
	    return;
	}
    }
    for(d=dumptab; d->d_func; d++) {
	printf("%c: %s\n", d->d_char, d->d_help);
    }
#endif /* SMALL_KERNEL */
}
#endif /* NO_INPUT_FROM_CONSOLE */

#ifdef __GNUC__
void
__main()
{
	/* GNU C generates a call to __main() from main() */
}
#endif

main()
{

  initialise();
  scavenger();
  assert(0);
  /*NOTREACHED*/
}
