/*	@(#)wtypes.h	1.2	94/04/06 17:23:02 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __WTYPES_H__
#define __WTYPES_H__

/*
**	wtypes.h:  collection of standard base types used throughout the
**		   window system and the kernel support routines.
*/

#ifdef __STDC__
#define NULL	((void *)0)
#else
#ifndef NULL
#define	NULL	0
#endif
#endif

/* sizing of rectangles */
#define	SIZEX(a)		(a.corner.x - a.origin.x)
#define	SIZEY(a)		(a.corner.y - a.origin.y)
#define	SIZEPX(a)		((a)->corner.x - (a)->origin.x)
#define	SIZEPY(a)		((a)->corner.y - (a)->origin.y)

#define	PT_IN_RECT(pt, r) \
	 (pt.x >= r.origin.x && pt.x < r.corner.x &&	\
	    pt.y >= r.origin.y && pt.y < r.corner.y)

#define	RECTXRECT(r1, r2) \
	 (r1.origin.x < r2.corner.x && r2.origin.x < r1.corner.x && \
	  r1.origin.y < r2.corner.y && r2.origin.y < r1.corner.y)

#define	NOT_LEGAL_RECT(r)	((r).origin.x >= (r).corner.x || \
				 (r).origin.y >= (r).corner.y)

#define	LEGAL_PRECT(r)		((r)->origin.x < (r)->corner.x && \
				 (r)->origin.y < (r)->corner.y)


/* related to the type Boolean */
#define	TRUE			1
#define	FALSE			0

#define	BITS_PER_BYTE	8
/* word size must be a power of 2 */
#define	WS		(sizeof(Word)*BITS_PER_BYTE)

typedef	char		Int8;	/* things that must be  8 bits */
typedef short		Int16;	/* things that must be 16 bits */
typedef long		Int32;	/* things that must be 32 bits */

typedef	int		Boolean;
typedef	Int16		Blt_op;
typedef	long		Colour;
typedef	unsigned long	Word;
typedef struct Point	Point;
typedef	struct Rect	Rect;
typedef	struct Bitmap	Bitmap;
typedef	struct Texture	Texture;

struct Point {
	Int16	x;
	Int16	y;
};

struct Rect {
	Point	origin;
	Point	corner;
};

struct Bitmap {
	Word *	base;
	Int16	width;
	Rect	rect;
#ifdef SEPARATE_VIDEO_MEMORY
	Boolean	in_vid;		/* true if bitmap is in video memory */
#endif
};

/*
** textures have a problem in that sometimes when a texture is passed
** we wish to indicate that there is no texture to be used in the
** operation.  to support this we have the valid field which is true
** when a texture is to be used and 0 otherwise.
*/
struct Texture {
	char	valid;
	Word	bits[WS];
};

#endif /* __WTYPES_H__ */
