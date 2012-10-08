/*	@(#)sunputchar.c	1.3	94/04/06 09:28:11 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	putchar
**		assumes that the console font is constant width and height
*/

#include "amoeba.h"
#include "window/wtypes.h"
#include "window/font.h"
#include "window/cursor.h"
#include "window/copy.h"

#define	CNTL(X)		((X) & 0x1f)

#ifdef __STDC__
#define COLINC(X)	cp->pos.x X##= fbx
#define ROWINC(X)	cp->pos.y X##= fby
#else
#define COLINC(X)	cp->pos.x X= fbx
#define ROWINC(X)	cp->pos.y X= fby
#endif

extern Cursor		Cur_tab[];
extern Font_ch *	Loc_cursor;

/* the built-in fonts have a separate Font_ch array: */
extern Font		def_Font, icon_Font;
extern Font_ch		def_Font_ch[], icon_Font_ch[];

sun_putchar(c)
{
  int			bitblt();
  int			erase();
  int			cursor_state();

  register Cursor *	cp = &Cur_tab[CONSOLE_CURSOR];
  register Font *	fp = cp->current_font;
  register Font_ch *	fchp = (fp == &def_Font) ? def_Font_ch :
			        ((fp == &icon_Font) ? icon_Font_ch :
				 (Font_ch *)(fp+1));
  register Int16	ox = cp->bitmap.rect.origin.x;
  register Int16	oy = cp->bitmap.rect.origin.y;
  register Int16	cx = cp->bitmap.rect.corner.x;
  register Int16	cy = cp->bitmap.rect.corner.y;
  register Int16	fbx = fchp->bmap.rect.corner.x;
  register Int16	fby = fchp->bmap.rect.corner.y;
  Font_index		ch;

  if (Loc_cursor != 0)
	cursor_state(CONSOLE_CURSOR, CUR_OFF);
  ch = c & 0xff;
  if (fp->first <= ch && ch <= fp->last)
  {
	Rect			dest;

	fchp += ch - fp->first;
	dest.origin = cp->pos;
	dest.corner.x = cp->pos.x + fbx;
	dest.corner.y = cp->pos.y + fby;
	bitblt(&fchp->bmap, fchp->bmap.rect.origin,
			&cp->bitmap, dest, (Texture*)0, SOURCE);
	COLINC(+);
  }
  else
	switch (c)
	{
	case '\f':
		COLINC(+);
		break;
	case '\r':
		cp->pos.x = ox;
		break;
	case '\b':
		COLINC(-);
		break;
	case '\n':
		ROWINC(+);
		break;
	case CNTL('G'):
		bleep();
		break;
	case CNTL('K'):
		ROWINC(-);
		break;
	case CNTL('Z'):
		erase();
		cp->pos.x = ox;
		cp->pos.y = oy;
		break;
	}

  if (cp->pos.x < ox)
	cp->pos.x = ox;
  else
	if (cp->pos.x >= cx)	/* wrap to a new line */
	{
		ROWINC(+);
		cp->pos.x = ox;
	}
  if (cp->pos.y < oy)
	cp->pos.y = oy;
  else
	if (cp->pos.y > (cy - ((cy-oy) % fby) - fby))
	{
		Rect		src;
		Rect		dst;	/* target scroll region */
		Rect		clr;	/* region to be cleared */

		dst = src = cp->bitmap.rect;
		src.origin.y += fby;
		dst.corner.y -= fby;
		clr.origin.x = dst.origin.x;
		clr.origin.y = dst.corner.y;
		clr.corner = cp->bitmap.rect.corner;

		vscroll(&cp->bitmap, src.origin, dst);
		bitblt(&cp->bitmap, clr.origin, &cp->bitmap, clr,
						(Texture *)0, D_ZEROS);
		ROWINC(-);
	}
  if (Loc_cursor != 0)
	cursor_state(CONSOLE_CURSOR, CUR_ON);
  return 0;
}


erase()
{
	int	bitblt();

	register Cursor * cp = &Cur_tab[ROOT_CURSOR];

	bitblt(&cp->bitmap, cp->bitmap.rect.origin, &cp->bitmap,
		cp->bitmap.rect, (Texture *)0, D_ZEROS);
}
