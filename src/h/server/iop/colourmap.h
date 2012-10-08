/*	@(#)colourmap.h	1.2	94/04/06 16:59:22 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __COLOURMAP_H__
#define	__COLOURMAP_H__

/*
 * This defines the basic structure which the X server will send to the
 * iop server to set a colour map on a colour display.  It may not yet
 * be sufficiently general so it may need to be altered in the future.
 * It is what the suns use, at any rate.
 */

typedef struct colour_map	colour_map;

struct colour_map {
	int	c_index;	/* offset in colour map */
	int	c_count;	/* # entries in each colour array */
	uint8 *	c_red;		/* pointer to array of red maps */
	uint8 *	c_green;	/* pointer to array of green maps */
	uint8 *	c_blue;		/* pointer to array of blue maps */
};

#endif /* __COLOURMAP_H__ */
