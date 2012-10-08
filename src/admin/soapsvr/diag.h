/*	@(#)diag.h	1.3	94/04/06 11:57:04 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef DIAG_H
#define DIAG_H

/*
 * Which soap replica are we:
 */
int   sp_whoami      _ARGS((void));
void  sp_set_whoami  _ARGS((int who));

/*
 * Put server status into a buffer for subsequent printing,
 * or answering a STD_STATUS request.
 */
char *buf_put_status _ARGS((char *buf, char *end));

/*
 * Diagnostics printing routines, in order of increased seriousness.
 */
void message	_ARGS((char *format, ...));
void scream	_ARGS((char *format, ...));
void panic	_ARGS((char *format, ...));

/* To synchronise printing: */
void start_printing _ARGS((void));
void stop_printing  _ARGS((void));

#endif
