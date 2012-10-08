/*	@(#)iopsvr.h	1.4	94/04/06 17:01:55 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __IOPSVR_H__
#define __IOPSVR_H__

/* Definitions of routines expected by server */

errstat iop_doenable _ARGS(( void ));
void	iop_dodisable _ARGS(( void ));
errstat iop_domousecontrol _ARGS(( IOPmouseparams * ));
errstat iop_doframebufferinfo _ARGS(( IOPFrameBufferInfo ** ));
errstat iop_domapmemory _ARGS(( capability *videoseg ));
errstat iop_dounmapmemory _ARGS(( capability *videoseg ));
errstat iop_dointdisable _ARGS(( void ));
errstat iop_dointenable _ARGS(( void ));
errstat iop_dokeyclick _ARGS(( int volume ));
errstat iop_dosetleds _ARGS(( long leds ));
errstat iop_dogetleds _ARGS(( long *leds ));
errstat iop_dobell _ARGS(( int volume, int pitch, int duration ));
errstat	iop_dosetcmap _ARGS(( colour_map * ));
errstat iop_dokbdtype _ARGS(( int * ));

/*
 * Definitions of callback routines in the server expected by the low-level
 * iop driver.
 */
void newevent _ARGS((int type, int keyorbut, int x, int y ));

#endif /* __IOPSVR_H__ */
