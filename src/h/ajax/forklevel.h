/*	@(#)forklevel.h	1.3	94/04/06 15:47:32 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __FORKLEVEL_H__
#define __FORKLEVEL_H__

/* Fork level interface */

extern mutex _ajax_forkmu; /* Protects critical section of fork() */
extern int _ajax_forklevel; /* Incremented in child after fork */

#endif /* __FORKLEVEL_H__ */
