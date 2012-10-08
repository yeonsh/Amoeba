/*	@(#)initfont.c	1.4	94/04/06 09:26:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	initialise the data structures necessary for the kernel without
**	the window system.
**	load the default font
** this must be done at boot time before putchar is used!!!
** this means it must be called before mpxinit() in main.c
*/

#include "amoeba.h"
#include "harddep.h"
#include "window/wtypes.h"
#include "window/font.h"
#include "window/cursor.h"
#include "window/attributes.h"
#include "window/locator.h"

extern struct Font    def_Font,      icon_Font;
extern struct Font_ch def_Font_ch[], icon_Font_ch[];
extern long 	      def_Bitmap[],  icon_Bitmap[];

static Bitmap	Screen;
Font_ch *	Loc_cursor;
Font *		Fp[2];
Cursor		Cur_tab[2];

font_boot()
{
	Cursor *		rootptr = &Cur_tab[ROOT_CURSOR];
	Cursor *		consptr = &Cur_tab[CONSOLE_CURSOR];
	register Font *		fp;
	register Font_ch *	fchp;
	register Word *		dsp;	/* data segment point for font */
	register int		i;

/* set up screen dimensions */
	Screen.rect.origin.x = SCR_ORG_X;
	Screen.rect.origin.y = SCR_ORG_Y;
	Screen.rect.corner.x = SCR_COR_X;
	Screen.rect.corner.y = SCR_COR_Y;
	Screen.width = (SCR_COR_X - SCR_ORG_X - 1) / WS + 1;
	Screen.base = SCREENBASE;
#ifdef SEPARATE_VIDEO_MEMORY
	Screen.in_vid = 1;
#endif

/* set up default font */
	Fp[DEFAULT_FONT] = &def_Font;
	fp = Fp[DEFAULT_FONT];
	fchp = def_Font_ch;
	dsp = (Word *)def_Bitmap;
	for (i = 0; i < fp->cnt;i++, fchp++)
		fchp->bmap.base = dsp + fchp->offset;
/* set up icon font */
	Fp[ICON_FONT] = &icon_Font;
	fp = Fp[ICON_FONT];
	fchp = icon_Font_ch;
	dsp = (Word *)icon_Bitmap;
	for (i = 0; i < fp->cnt;i++, fchp++)
		fchp->bmap.base = dsp + fchp->offset;
/*
**	initialise the 0th cursor (ROOT CURSOR) and 1st cursor (CONSOLE CURSOR)
*/
	rootptr->parent = (Cursor *)NULL;
	rootptr->first_child = consptr;
	rootptr->next_sibling = (Cursor *)NULL;
	rootptr->active = CUR_ACTIVE;
	rootptr->state = CUR_OFF;
	rootptr->visibility = CUR_INVISIBLE;
	rootptr->bitmap = Screen;
	rootptr->boundary = Screen.rect;
	rootptr->current_font = Fp[DEFAULT_FONT];
/* no writing occurs to root window */

	consptr->parent = rootptr;
	consptr->first_child = (Cursor *)NULL;
	consptr->next_sibling = (Cursor *)NULL;
	consptr->active = CUR_ACTIVE;
	consptr->pos = Screen.rect.origin;
	consptr->boundary = Screen.rect;
	consptr->line_type = L_SOLID;
	consptr->marker_type = M_CROSS;
	consptr->visibility = CUR_VISIBLE;
	consptr->state = CUR_OFF;
	consptr->bitmap = Screen;
	consptr->current_font = Fp[DEFAULT_FONT];
/* the zeroth character of the default font is the cursor */
	consptr->icon = icon_Font_ch + DEFAULT_TEXT_CURSOR;

/*
** since the locator cursor is not used, a dummy (non-zero) value can be
** used in this case
*/
	Loc_cursor = (Font_ch *)0x100;
}

#ifdef IOP
home_cursor()
{
    cursor_state(CONSOLE_CURSOR, CUR_OFF);
    Cur_tab[CONSOLE_CURSOR].pos = Screen.rect.origin;
    cursor_state(CONSOLE_CURSOR, CUR_ON);
}
#endif
