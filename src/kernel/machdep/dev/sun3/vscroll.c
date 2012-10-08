/*	@(#)vscroll.c	1.2	94/04/06 09:29:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	vscroll
**
**	performs vertical scrolling in a manner hopefully faster than
**	bitblt.
**
**	bmap.rect is assumed to be clipped to the clipping rectangle
**	or d_rect must be clipped to the clipping rectangle
*/

#include "window/wtypes.h"
#include "window/copy.h"

#define	ALLONES		((Word)~0)

#define	CLIP(src_o, src_c, src_pt, dest_o, dest_c, clip_o, clip_c) \
{									\
    register Int16      t;						\
/* shift src_pt to be in src bitmap */					\
    t = src_pt; t -= src_o;						\
    if (t < 0)								\
    {									\
        src_pt = src_o;							\
        clip_o -= t;							\
    }									\
/* shift src_pt so that it is in destmap */				\
    t = clip_o; t -= dest_o;						\
    if (t < 0)								\
    {									\
        clip_o = dest_o;						\
        src_pt -= t;							\
    }									\
/* clip must lie within dest bitmap */					\
    t = clip_c; t -= dest_c;						\
    if (t > 0)								\
        clip_c = dest_c;						\
/* sizeof dest must be <= size of src */				\
    t = clip_c; t -= clip_o; t -= src_c; t += src_pt;			\
    if (t > 0)								\
        clip_c -= t;							\
}

#define	MASK1 \
	*d_ptr = (mask1 & *s_ptr++) | (~mask1 & *d_ptr);	\
	d_ptr++;

#define	MASK2 \
	*d_ptr = (mask2 & *s_ptr++) | (~mask2 & *d_ptr);	\
	d_ptr++;

/*
**  s_ptr and d_ptr are cast to (char *) so that delta is not multiplied
**  by sizeof word during the loop.  The multiplication of delta
**  happens only once - when delta is calculated.
*/
#define	DO_SCROLL(M1, M2) \
{								\
    if (n_words >= 0)						\
	do							\
	{							\
	    M1							\
	    word = n_words;					\
	    do							\
		*d_ptr++ = *s_ptr++;				\
	    while (--word != -1);				\
	    M2							\
	    s_ptr = (Word *)((char *)s_ptr + delta);		\
	    d_ptr = (Word *)((char *)d_ptr + delta);		\
	} while (--row != -1);					\
    else							\
	do							\
	{							\
	    M1							\
	    M2							\
	    s_ptr = (Word *)((char *)s_ptr + delta);		\
	    d_ptr = (Word *)((char *)d_ptr + delta);		\
	} while (--row != -1);					\
}

vscroll(bmap, s_origin, d_rect)
register Bitmap *	bmap;		/* source bitmap */
Point			s_origin;	/* start point within bitmap */
Rect			d_rect;		/* target area in bitmap */
{
	register Word *	s_ptr;
	register Word *	d_ptr;
	register Word	mask1;
	register Word	mask2;
	register int	n_words;	/* # words in the row to be blitted */
	register short	row;

/*
**	CLIP
*/
	CLIP(bmap->rect.origin.x, bmap->rect.corner.x,
		s_origin.x, bmap->rect.origin.x, bmap->rect.corner.x,
		d_rect.origin.x, d_rect.corner.x);

	CLIP(bmap->rect.origin.y, bmap->rect.corner.y, s_origin.y,
	     bmap->rect.origin.y, bmap->rect.corner.y,
	     d_rect.origin.y, d_rect.corner.y);
	
/* put width, leftend in registers */

    {	register Int16	width;		/* width  of destination in pixels */
	register int	leftend;

	width = d_rect.corner.x;
	width -= d_rect.origin.x;
	if (--width < 0)
		return;	/* INVALID RECTANGLE */
/* misuse the row variable to calculate the height */
	row = d_rect.corner.y;
	row -= d_rect.origin.y;
	if (--row < 0)
		return;	/* INVALID RECTANGLE */

/*
**	CALCULATE EDGE MASKS
*/

/* # bits in 1st word of row that take part in the operation */
	leftend = d_rect.origin.x;
	leftend &= (WS-1);
/* mask for 1st word in row */
	mask1 = ALLONES >> leftend;
/* mask for last word in row */
/* since WS-1 is of the form 2^n - 1 (ie modulo operator) we can use
** leftend in place of d_rect.origin.x here
**	mask2 = ALLONES << (WS - 1 - ((d_rect.origin.x + width) & (WS-1)));
*/
	mask2 = ALLONES << (WS - 1 - ((leftend + width) & (WS-1)));
/* number of words in the row */
	leftend += width;
	leftend -= WS;
	if (leftend < 0)	/* fits in 1 word! */
	{
		mask1 &= mask2;
		mask2 = 0;
		n_words = 1;
	}
	else
	{
		n_words = leftend;
		n_words /= WS;
		n_words += 2;
	}
    } /* end width , leftend in registers */

/*
**	CHECK FOR OVERLAP OF SRC AND DEST
*/
	/* put delta and word count into registers */
    {	register short	word;
	register int	delta;

/* if the SRC bitmap is not used in the operation */
	if (d_rect.origin.y > s_origin.y)	/* start at bottom */
	{
		s_origin.y += row;
		d_rect.origin.y += row;
		delta = -bmap->width;
	}
	else
		delta = bmap->width;
	delta -= n_words;
/*
** do the increment now so that incrementing of the (Word *) pointers
** can be done with a single addition!
*/
	delta *= sizeof(Word);

/*
**	CALCULATE OFFSETS FROM BITMAP BASES
*/
	s_ptr = bmap->base + (s_origin.y * bmap->width + s_origin.x / WS);
	d_ptr = bmap->base + (d_rect.origin.y * bmap->width + d_rect.origin.x / WS);
/*
**	COPY
*/
	if (mask2 == ALLONES)
	{
		mask2 = 0;
		n_words -= 2;
	}
	else
		n_words -= 3;
/*
**	COPY LOOP
*/
	if (mask1 == ALLONES)
	{
		n_words++;
		if (mask2 == 0)
			DO_SCROLL(/**/, /**/)
		else
			DO_SCROLL(/**/, MASK2)
	}
	else
		if (mask2 == 0)
			DO_SCROLL(MASK1, /**/)
		else
			DO_SCROLL(MASK1, MASK2)
    }
}
