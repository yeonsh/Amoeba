/*	@(#)random.h	1.2	94/04/06 17:05:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __RANDOM_H__
#define __RANDOM_H__

/*
** Random number server constants
*/

/* directory entry name in processor directory */
#define	RANDOM_SVR_NAME	"random"

#define MAX_RANDOM	1024

/* commands accepted by the random server */
#define RND_GETRANDOM	(RND_FIRST_COM)

#endif /* __RANDOM_H__ */
