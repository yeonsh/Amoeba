/*	@(#)locator.h	1.2	94/04/06 17:22:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __LOCATOR_H__
#define __LOCATOR_H__

/*
**	Locator trap information
*/

#define	MAX_BUTTONS	3	/* number of buttons on the virtual mouse */
#define	MAX_TRAP_ID	144	/* max no. simultaneous traps(3*maxwindows) */

/*
**	locator cursor information
*/

#define	CREATED		1
#define	DELETED		0

#define	ANY		1
#define	RELEASE		0

#define	WIEM_CURSOR	((Font_index)6)
#define	FRAME_CURSOR	((Font_index)7)
#define	IMAGE_CURSOR	((Font_index)8)
#define	DANGER_CURSOR	((Font_index)9)

typedef	struct Locat_trap	Locat_trap;
typedef	struct Locator_state	Locator_state;
typedef	Int16			Loc_trap_id;

/*
**	The fields of the Loc_trap struct are as follows:
**	  the trap region
**	  was the mouse inside the region when the mouse was last updated
**	  is the trap set
**	  capability for the trap
*/
struct Locat_trap {
	Rect		rect;
	Int8		status;
	Int8		active;
	port		random;
	capability	cap;
};

struct Locator_state {
	Point		pos;
	char		buttons[MAX_BUTTONS];
};

#endif /* __LOCATOR_H__ */
