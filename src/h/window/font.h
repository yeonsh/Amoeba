/*	@(#)font.h	1.2	94/04/06 17:22:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __FONT_H__
#define __FONT_H__

/*
**	font.h
**		each character in a font can be a different size and
**		may possibly be located in a different type of memory
**		depending on availablility of the various types.
*/

#define	FONT_SERVER_PATH	"public/fontserver"

#define	MAX_FONT_SIZE	30000	/* size of largest font in bytes */
#define	MAX_FONTS	4	/* total number of mountable fonts */
#define	MAX_TAB		128	/* maximum number of fonts in cache table */

#define	FNT_LOAD		31	/* command to font server */
#define	FNT_REGISTER		99	/* command to font server */
#define	FNT_SUCCESS		77	/* request succeeded */
#define	FNT_UNKNOWN		32	/* error message */
#define	FNT_NOTFOUND		15	/* error message */
#define	FNT_SIZE		11	/* error message */
#define	FNT_FAIL		10	/* error message */

/* some font numbers */
#define	DEFAULT_FONT	0
#define	ICON_FONT	1

typedef Int16		Font_index;
typedef struct Font_ch	Font_ch;
typedef struct Font	Font;

struct Font
{
    char	name[16];	/* name that font is registered under */
    port	encoded;	/* encoded random part of capability */
    Int16	cnt;		/* # characters in font */
    Int16	ptsize;		/* imagined point size */
    Int16	first;		/* first character in font */
    Int16	last;		/* last character in font */
    Int16	space;		/* the space character in the font */
    Int16	ref_count;	/* indicator of interest in the font */
};

/*
** note that the font bitmap always starts at (0,0) so the width and
** height of the bitmap are found in bmap.rect.corner.[xy]
*/
struct Font_ch
{
    Bitmap	bmap;		/* the data of the character */
    Point	base_pt;	/* lower left point of char on baseline */
    int		offset;		/* address offset of bitmap from font base */
};

#endif /* __FONT_H__ */
