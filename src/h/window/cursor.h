/*	@(#)cursor.h	1.2	94/04/06 17:22:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __CURSOR_H__
#define __CURSOR_H__

/*
** cursor management information
**
**	the parent/child relationships are kept to describe cursors within
**	another cursor's clip region
*/
#define	MAX_CURSORS	256

/* position of the default cursor in the icon font */
#define	DEFAULT_TEXT_CURSOR	0
#define	DEFAULT_LOC_CURSOR	6

/* special cursors */
#define	ROOT_CURSOR	0
#define	CONSOLE_CURSOR	1

#define	CUR_ON		1
#define	CUR_OFF		0

#define	CUR_ACTIVE	1
#define	CUR_FREE	0

#define	CUR_INVISIBLE	0
#define	CUR_VISIBLE	1
#define	CUR_HIGHLIGHTED	2

typedef	Int16			Cursor_index;
typedef struct Cursor		Cursor;
struct Cursor {
	Int8		active;		/* !0 => cursor is in use */
	Int8		visibility;	/* required vis. of the cursor */
	Int8		state;		/* is it presently visible? */
	Int8		line_type;
	Int8		line_width;
	Int8		text_alignment;
	Int8		text_orientation;
	Int8		text_path;
	Int16		verticalspace;
	Font_index	marker_type;
	Cursor *	parent;		/* parent cursor of this one */
	Cursor *	next_sibling;	/* is there a brother window? */
	Cursor *	first_child;	/* is there a sub-window? */
	Point		pos;
	Font_ch *	icon;
	Font_ch *	fill_pattern;
	Font *		current_font;
	Rect		boundary;	/* maximum clip for this cursor */
	Bitmap		bitmap;		/* bitmaps comprising this cursor */
};

#endif /* __CURSOR_H__ */
