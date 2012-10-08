/*	@(#)alloc.h	1.2	96/02/27 13:01:17 */
/*
 * area-based allocation built on malloc/free
 */

typedef struct Area {
	struct Block *free;	/* free list */
} Area;

