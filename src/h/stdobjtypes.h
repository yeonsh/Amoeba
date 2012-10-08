/*	@(#)stdobjtypes.h	1.2	94/04/06 15:46:19 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
**  This file contains the definitions of the symbols used by servers
**  in their stdinfo string to identify the object.
**  Note:  only objects are identified this way.  Servers for the object
**  describe themselves with a longer string at present, although they
**  could be of the object type server and return S followed by the symbol
**  of the type of object.
*/

#define	OBJSYM_TTY	"+"	/* tty */
#define	OBJSYM_BULLET	"-"	/* bullet file */
#define OBJSYM_DIRECT	"/"	/* directory */
#define	OBJSYM_KERNEL	"%"	/* kernel directory */
#define	OBJSYM_DISK	"@"	/* disk, virtual or otherwise */
#define	OBJSYM_PROCESS	"!"	/* process, running or otherwise */

/* possible extensions? */
#define	OBJSYM_PIPE	"|"
#define	OBJSYM_RANDOM	"?"	/* random number?? */
