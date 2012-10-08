/*	@(#)bitblt.c	1.3	94/04/06 09:24:58 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	Bitblt
**
**	performs the operation:
**		dest cut d_rect op <= src {with texture} from s_origin
**
**	dest.rect is assumed to be clipped to the clipping rectangle
**	or d_rect must be clipped to the clipping rectangle
*/

#include "window/wtypes.h"
#include "window/copy.h"

#define	ALLONES		((Word)~0)

#ifdef ASM_ROLL_D7_D6
/* optimisation (probably) only valid for the original Sun compiler: */
#define	ROTATE_LEFT(w, s)	asm("roll d7,d6")
#else /* generic case: */
#define	ROTATE_LEFT(w, s)	w = (((w) << (s)) | ((w) >> (WS - (s))))
#endif

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

#define	SKEW_SIMPLE(X) \
    prevword = *(X);				\
    skewword = prevword & ~skewmask;		\
    ROTATE_LEFT(skewword, skew);

#define	SKEW_SOURCE(X) \
    thisword = *(X);					\
    prevword &= skewmask;				\
    skewword = prevword | (thisword & ~skewmask);	\
    prevword = thisword;				\
    ROTATE_LEFT(skewword, skew);

#define	DO_TEXT(RESULT, SPTR, DEC_DST, INC_DST) \
{									\
    do									\
    {									\
	textword = texture->bits[step];					\
	step = (step + v_dir) & (WS -1);				\
	SKEW_SIMPLE(SPTR);						\
	thisword = mask1;						\
	DEC_DST;							\
	*d_ptr = (thisword & (RESULT)) | (~thisword & *d_ptr);		\
	INC_DST;							\
	if ((word = n_words) >= 0)					\
	    do								\
	    {								\
		SKEW_SOURCE(SPTR);					\
		DEC_DST;						\
		*d_ptr = (RESULT);					\
		INC_DST;						\
	    } while (--word != -1);					\
	if (mask2 != 0)							\
	{								\
	    SKEW_SOURCE(SPTR);						\
	    thisword = mask2;						\
	    DEC_DST;							\
	    *d_ptr = (thisword & (RESULT)) | (~thisword & *d_ptr);	\
	    INC_DST;							\
	}								\
	s_ptr = (Word *) ((char *) s_ptr + s_delta);			\
	d_ptr = (Word *) ((char *) d_ptr + d_delta);			\
    } while (--row != -1);						\
}

#define	DO_PRETEXT(RESULT, SPTR, DEC_DST, INC_DST) \
{									\
    do									\
    {									\
	textword = texture->bits[step];					\
	step = (step + v_dir) & (WS -1);				\
	prevword = *(SPTR);						\
	SKEW_SOURCE(SPTR);						\
	thisword = mask1;						\
	DEC_DST;							\
	*d_ptr = (thisword & (RESULT)) | (~thisword & *d_ptr);		\
	INC_DST;							\
	if ((word = n_words) >= 0)					\
	    do								\
	    {								\
		SKEW_SOURCE(SPTR);					\
		DEC_DST;						\
		*d_ptr = (RESULT);					\
		INC_DST;						\
	    } while (--word != -1);					\
	if (mask2 != 0)							\
	{								\
	    SKEW_SOURCE(SPTR);						\
	    thisword = mask2;						\
	    DEC_DST;							\
	    *d_ptr = (thisword & (RESULT)) | (~thisword & *d_ptr);	\
	    INC_DST;							\
	}								\
	s_ptr = (Word *) ((char *) s_ptr + s_delta);			\
	d_ptr = (Word *) ((char *) d_ptr + d_delta);			\
    } while (--row != -1);						\
}

#define	DO_UNTEXT(RESULT, SPTR, DEC_DST, INC_DST) \
{									\
    do									\
    {									\
	SKEW_SIMPLE(SPTR);						\
	thisword = mask1;						\
	DEC_DST;							\
	*d_ptr = (thisword & (RESULT)) | (~thisword & *d_ptr);		\
	INC_DST;							\
	if ((word = n_words) >= 0)					\
	    do								\
	    {								\
		SKEW_SOURCE(SPTR);					\
		DEC_DST;						\
		*d_ptr = (RESULT);					\
		INC_DST;						\
	    } while (--word != -1);					\
	if (mask2 != 0)							\
	{								\
	    SKEW_SOURCE(SPTR);						\
	    thisword = mask2;						\
	    DEC_DST;							\
	    *d_ptr = (thisword & (RESULT)) | (~thisword & *d_ptr);	\
	    INC_DST;							\
	}								\
	s_ptr = (Word *) ((char *) s_ptr + s_delta);			\
	d_ptr = (Word *) ((char *) d_ptr + d_delta);			\
    } while (--row != -1);						\
}

#define	DO_UNTXTCHR(RESULT, SPTR, DEC_DST, INC_DST) \
{								\
    do								\
    {								\
	SKEW_SIMPLE(SPTR);					\
	thisword = mask1;					\
	DEC_DST;						\
	*d_ptr = (thisword & (RESULT)) | (~thisword & *d_ptr);	\
	INC_DST;						\
	d_ptr = (Word *) ((char *) d_ptr + d_delta);		\
    } while (--row != -1);					\
}

#define	DO_PREUNTEXT(RESULT, SPTR, DEC_DST, INC_DST) \
{									\
    do									\
    {									\
	prevword = *(SPTR);						\
	SKEW_SOURCE(SPTR);						\
	thisword = mask1;						\
	DEC_DST;							\
	*d_ptr = (thisword & (RESULT)) | (~thisword & *d_ptr);		\
	INC_DST;							\
	if ((word = n_words) >= 0)					\
	    do								\
	    {								\
		SKEW_SOURCE(SPTR);					\
		DEC_DST;						\
		*d_ptr = (RESULT);					\
		INC_DST;						\
	    } while (--word != -1);					\
	if (mask2 != 0)							\
	{								\
	    SKEW_SOURCE(SPTR);						\
	    thisword = mask2;						\
	    DEC_DST;							\
	    *d_ptr = (thisword & (RESULT)) | (~thisword & *d_ptr);	\
	    INC_DST;							\
	}								\
	s_ptr = (Word *) ((char *) s_ptr + s_delta);			\
	d_ptr = (Word *) ((char *) d_ptr + d_delta);			\
    } while (--row != -1);						\
}

#define	SRC		(skewword & textword)
#define	DESTINATION	(*d_ptr)

#define	DO_BLT(NOTS, OP, NOTD, DST) \
{									\
    register Word	thisword;					\
    register Word	prevword;					\
    register short	word;						\
    if (h_dir > 0)							\
    {									\
	if (texture == 0)						\
	    if (preload)						\
		DO_PREUNTEXT((NOTS (skewword) OP NOTD DST), s_ptr++, /**/, d_ptr++)\
	    else							\
		if (n_words < 0 && mask2 == 0 && s_delta == 0)		\
		    DO_UNTXTCHR((NOTS (skewword) OP NOTD DST), s_ptr++, /**/, d_ptr++)\
		else							\
		    DO_UNTEXT((NOTS (skewword) OP NOTD DST), s_ptr++, /**/, d_ptr++)\
	else								\
	    if (preload)						\
		DO_PRETEXT((NOTS SRC OP NOTD DST), s_ptr++, /**/, d_ptr++) \
	    else							\
		DO_TEXT((NOTS SRC OP NOTD DST), s_ptr++, /**/, d_ptr++)	\
    }									\
    else								\
    {									\
	s_ptr++; d_ptr++;						\
	if (texture == 0)						\
	    if (preload)						\
		DO_PREUNTEXT((NOTS skewword OP NOTD DST), --s_ptr, --d_ptr, /**/)\
	    else							\
		DO_UNTEXT((NOTS skewword OP NOTD DST), --s_ptr, --d_ptr, /**/)\
	else								\
	    if (preload)						\
		DO_PRETEXT((NOTS SRC OP NOTD DST), --s_ptr, --d_ptr, /**/)\
	    else							\
		DO_TEXT((NOTS SRC OP NOTD DST), --s_ptr, --d_ptr, /**/)\
    }									\
}


/* h_dir is always positive in the fast case */
#define	DO_FAST(RESULT) \
{								\
    register short	word;					\
    register Word	mask;					\
    do								\
    {								\
	mask = mask1;						\
	*d_ptr = (mask & (RESULT)) | (~mask & *d_ptr);		\
	d_ptr++;						\
	if ((word = n_words) >= 0)				\
	    do							\
	    {							\
		*d_ptr = (RESULT);				\
		d_ptr++;					\
	    } while (--word != -1);				\
	if ((mask = mask2) != 0)				\
	{							\
	    *d_ptr = (mask & (RESULT)) | (~mask & *d_ptr);	\
	    d_ptr++;						\
	}							\
	d_ptr = (Word *) ((char *) d_ptr + d_delta);		\
    } while (--row != -1);					\
}


#define	DO_FTX(RESULT) \
{								\
    register short	word;					\
    register Word	mask;					\
    do								\
    {								\
	textword = texture->bits[step];				\
	step = (step + v_dir) & (WS -1);			\
	mask = mask1;						\
	*d_ptr = (mask & (RESULT)) | (~mask & *d_ptr);		\
	d_ptr++;						\
	if ((word = n_words) >= 0)				\
	    do							\
	    {							\
		*d_ptr = (RESULT);				\
		d_ptr++;					\
	    } while (--word != -1);				\
	if ((mask = mask2) != 0)				\
	{							\
	    *d_ptr = (mask & (RESULT)) | (~mask & *d_ptr);	\
	    d_ptr++;						\
	}							\
	d_ptr = (Word *) ((char *) d_ptr + d_delta);		\
    } while (--row != -1);					\
}

#define	DO_FST_TXT(RESULT) \
{								\
    if (texture == 0)						\
	DO_FAST(RESULT)						\
    else							\
	DO_FTX(RESULT & textword)				\
}

bitblt(sbp, s_origin, dbp, d_rect, text, op)
Bitmap *		sbp;		/* source bitmap */
Point			s_origin;	/* start point within src bitmap */
Bitmap *		dbp;		/* destination bitmap */
Rect			d_rect;		/* target/clip area in dest bitmap */
Texture *		text;		/* pattern to operate against */
Blt_op			op;		/* bitblt operator */
{
    register Word *	s_ptr;
    register Word *	d_ptr;
    register int	skew;		/* known to be d7! */
    register Word	skewword;	/* known to be d6! */
    register Word	skewmask;
    Word		textword;	/* texture word */
    Word		mask1;
    Word		mask2;
    int			n_words;	/* # words in the row to be blitted */
    int			h_dir;		/* horizontal blit direction */
    int			v_dir;		/* vertical blit direction */
    Int16		height;		/* height of destination in pixels */
    int			s_delta;
    int			d_delta;
    int			step;
    Boolean		preload;

/*
**	CLIP
*/
    {	register Bitmap * src = sbp;
	register Bitmap * dest = dbp;

	CLIP(src->rect.origin.x, src->rect.corner.x,
		s_origin.x, dest->rect.origin.x, dest->rect.corner.x,
		d_rect.origin.x, d_rect.corner.x);

	CLIP(src->rect.origin.y, src->rect.corner.y, s_origin.y,
	     dest->rect.origin.y, dest->rect.corner.y,
	     d_rect.origin.y, d_rect.corner.y);
	
/* put width in register */
    {   register Int16	width;	/* width  of destination in pixels */

	width = d_rect.corner.x;
	width -= d_rect.origin.x;
	if (--width < 0)
		return;
	height = d_rect.corner.y - d_rect.origin.y -1;
	if (height < 0)
		return;	/* INVALID RECTANGLE */

/*
**	CALCULATE SKEWING MASKS
*/
    {	register int	leftend;

/* # bits in 1st word of row that take part in the operation */
	leftend = d_rect.origin.x;
	leftend &= (WS-1);
/* mask for 1st word in row */
	mask1 = ALLONES >> leftend;
/* mask for last word in row */
/*
	mask2 = ALLONES << (WS - 1 - ((d_rect.origin.x + width) & (WS-1)));
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
		leftend /= WS;
		leftend += 2;
		n_words = leftend;
	}
    } /* end leftend in register */

/* skew for the row */
    skew = s_origin.x;
    skew -= d_rect.origin.x;
    skew &= (WS-1);
    skewmask = skew == 0 ? 0 : (ALLONES >> skew);

/*
**	CHECK FOR OVERLAP OF SRC AND DEST
*/

/* if the SRC bitmap is not used in the operation */
    h_dir = v_dir = 1;
    if (op == D_ZEROS || op == D_ONES || op == DEST || op == NOT_D)
	preload = 0;
    else
    {
	if (src->base == dest->base && d_rect.origin.y >= s_origin.y)
	    if (d_rect.origin.y > s_origin.y)	/* start at bottom */
	    {
		v_dir = -1;
		s_origin.y += height;
		d_rect.origin.y += height;
	    }
	    else	  /* y's are equal but x's are backward */
	    {	  /* don't swap horizontal direction if only one word */
		if (n_words > 1 && d_rect.origin.x > s_origin.x)
		{
		    register Word	tmp;	/* for swapping masks */

		    h_dir = -1;
		    s_origin.x += width;
		    d_rect.origin.x += width;
		    skewmask = ~skewmask;
		    tmp = mask1;
		    mask1 = mask2;
		    mask2 = tmp;
		}
	    }

/*
**	CALCULATE OFFSETS FROM BITMAP BASES
*/
	preload = skew != 0 && skew <= (s_origin.x & (WS-1));
	if (h_dir < 0)
		preload = !preload;
	s_ptr = src->base + (s_origin.y * src->width + s_origin.x / WS);
	s_delta = (src->width * v_dir - (n_words + preload) * h_dir) *sizeof(Word);
    } /* end else */
    d_ptr = dest->base + (d_rect.origin.y * dest->width + d_rect.origin.x / WS);
    d_delta = (dest->width * v_dir - n_words * h_dir) * sizeof(Word);

    } /* end width in register */
    }/*end src and dest in registers */
/*
**	COPY
*/
    step = d_rect.origin.y & (WS-1);
    if (mask2 == ALLONES)
    {
	mask2 = 0;
	n_words -= 2;
    }
    else
	n_words -= 3;

    {   register Texture *	texture = text;
	register short	row;

	row = height;
	switch (op)
	{
	case D_ZEROS:		skewword = 0;
				DO_FAST(skewword);
				break;
	case S_AND_D:		DO_BLT(/**/, &, /**/, DESTINATION);
				break;
	case S_AND_NOT_D:	DO_BLT(/**/, &, ~, DESTINATION);
				break;
	case SOURCE:		DO_BLT(/**/, /**/, /**/, /**/);
				break;
	case NOT_S_AND_D:	DO_BLT(~, &, /**/, DESTINATION);
				break;
	case DEST:
				break;
	case S_XOR_D:		DO_BLT(/**/, ^, /**/, DESTINATION);
				break;
	case S_OR_D:		DO_BLT(/**/, |, /**/, DESTINATION);
				break;
	case NOT_S_AND_NOT_D:	DO_BLT(~, &, ~, DESTINATION);
				break;
	case NOT_S_XOR_D:	DO_BLT(~, ^, /**/, DESTINATION);
				break;
	case NOT_D:		DO_FST_TXT(~DESTINATION);
				break;
	case S_OR_NOT_D:	DO_BLT(/**/, |, ~, DESTINATION);
				break;
	case NOT_S:		DO_BLT(~, /**/, /**/, /**/);
				break;
	case NOT_S_OR_D:	DO_BLT(~, |, /**/, DESTINATION);
				break;
	case NOT_S_OR_NOT_D:	DO_BLT(~, |, ~, DESTINATION);
				break;
	case D_ONES:		skewword = ALLONES;
				DO_FST_TXT(skewword);
				break;
	}
    }/* end texture ptr and row in a register */
}
