/*	@(#)attributes.h	1.2	94/04/06 17:22:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ATTRIBUTES_H__
#define __ATTRIBUTES_H__

/*
**	attributes.h - various attributes for fonts, lines, etc.
*/

/* size of the packet returned by the inquire attributes command */
#define ATTR_SZ (8*sizeof (Int8) + sizeof (Int16) + sizeof (Rect) \
			+ sizeof (Point) + sizeof (Font_index))

typedef	struct attr_packet	Attr_p;
struct attr_packet {
	Rect		bmap_rect;
	Point		pos;
	Font_index	marker_type;
	Int16		vertical_space;
	Int8		visibility;
	Int8		line_type;
	Int8		line_width;
	Int8		text_alignment;
	Int8		text_orientation;
	Int8		text_path;
	Int8		font_number;
	Int8		bell_type;
};


/* valid attributes */
#define	LINETYPE		0
#define	LINEWIDTH		1
#define	MARKERTYPE		2
#define	TEXT_ALIGNMENT		3
#define	TEXT_ORIENTATION	4
#define	TEXT_PATH		5
#define	CURSOR_VISIBILITY	6
#define	VERTICAL_SPACE		7
#define	LOCATOR_ORIENTATION	10
#define	BELL_TYPE		11

/* Line Types */
#	define	L_SOLID			1
#	define	L_DASH			2
#	define	L_DOT			3
#	define	L_DASH_DOT		4
#	define	L_DASH_DOUBLE_DOT	5


/* Marker types */
#	define	M_DOT			1
#	define	M_PLUS			2
#	define	M_ASTERISK		3
#	define	M_CIRCLE		4
#	define	M_CROSS			5

/* bell types */
#	define	NO_BELL			0
#	define	VISIBLE_BELL		1
#	define	AUDIBLE_BELL		2

#endif /* __ATTRIBUTES_H__ */
