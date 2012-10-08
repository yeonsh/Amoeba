/*	@(#)cursor_state.c	1.2	94/04/06 09:25:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**  Switch a cursor on or off!
**	This assumes that there are no sub cursors (since the window system
**	is not involved - only window 1
*/

#include "amoeba.h"
#include "window/wtypes.h"
#include "window/font.h"
#include "window/cursor.h"
#include "window/locator.h"
#include "window/copy.h"

extern Cursor		Cur_tab[];

cursor_state(cur, stat)
Cursor_index	cur;
Int16		stat;
{
    int		bitblt();

    Point		pt;		/* src of cursor icon */
    Rect		dst;		/* dst of cursor (on screen) */
    register Cursor *	cp = &Cur_tab[cur];

    if (cp->visibility != CUR_INVISIBLE && cp->state != stat)
    {
	pt = cp->icon->bmap.rect.origin;
	dst.origin = cp->pos;
	dst.corner.x = cp->pos.x + cp->icon->bmap.rect.corner.x;
	dst.corner.y = cp->pos.y + cp->icon->bmap.rect.corner.y;
	bitblt(&(cp->icon->bmap), pt, &(cp->bitmap), dst, (Texture *)0, S_XOR_D);
    }
    cp->state = stat;
}
