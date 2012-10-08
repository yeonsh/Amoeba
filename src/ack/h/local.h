/*	@(#)local.h	1.2	93/01/15 17:10:01 */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/* collection of options, selected by including or excluding 'defines' */

/* Version number of the EM object code */
#	define	VERSION		3	/* 16 bits number */

/* The default machine used by ack, acc, apc */
#ifndef ACKM
#	define	ACKM		"sun3"
#endif

/* size of local machine, either 0 (for 16 bit address space), or 1 */
#	define BIGMACHINE	1

/* operating system, SYS_5, V7, BSD4_1 or BSD4_2; Do NOT delete the comment
   in the next line! */
#	define BSD4_2		1	/* SYSTEM */
