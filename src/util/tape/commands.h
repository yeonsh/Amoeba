/*	@(#)commands.h	1.2	94/04/07 16:10:47 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** mt.h, include file for mt program.
**
*/
/*
** definitions.
**
*/
#define READ		0	/* Perform a write */
#define WRITE		1	/* Perform a read */
#define UNLOAD		2	/* Abort last command */
#define ERASE		3	/* Erase tape */
#define FSKIP		4	/* Skip files on tape */
#define RSKIP		5	/* Skip records on tape */
#define LOAD		6	/* Initialize unit */
#define REWIND		7	/* Rewind tape */
#define STATUS		8	/* Dump tape status on stdout */

/*
** definitions.
**
*/
#define MAX_NUM_BUFS	100
