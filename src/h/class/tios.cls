/*	@(#)tios.cls	1.3	96/02/27 10:29:28 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* Class tios implements the Posix termios interface for tty drivers */

#include "amoeba.h"
#include "cmdreg.h"
#include "posix/termios.h"

class tios [TIOS_FIRST_COM .. TIOS_LAST_COM] {
	include "posix/termios.h";
	
	/* These constants are supposed to correspond to the functions */
	const TIOS_GETATTR = TIOS_FIRST_COM + 0;
	const TIOS_SETATTR = TIOS_FIRST_COM + 1;
	const TIOS_SENDBREAK = TIOS_FIRST_COM + 2;
	const TIOS_DRAIN = TIOS_FIRST_COM + 3;
	const TIOS_FLUSH = TIOS_FIRST_COM + 4;
	const TIOS_FLOW = TIOS_FIRST_COM + 5;
	const TIOS_GETWSIZE = TIOS_FIRST_COM + 6;
	const TIOS_SETWSIZE = TIOS_FIRST_COM + 7;
	
	tios_getattr(*, out struct termios tp);
	tios_setattr(*, in:h_extra int optional_actions,
			var in struct termios tp);
	tios_sendbrk(*, in:h_extra int duration);
	tios_drain(*);
	tios_flush(*, in:h_extra int queue_selector);
	tios_flow(*, in:h_extra int action);
	tios_getwsize(*, out int width, out int length);
	tios_setwsize(*, in int width, in int length);
};

class tios1 {
	include "posix/termios.h";
	/* A typedef so we can generate server marshalers */
	typedef struct termios termios;
	marshal termios with	/**/
			in	/**/, tc_unmarshal
			out	/**/, tc_marshal;
};
