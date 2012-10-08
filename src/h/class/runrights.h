/*	@(#)runrights.h	1.2	94/04/06 15:50:27 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef _RUN_RIGHTS_H
#define _RUN_RIGHTS_H

/* run server rights */
#define RUN_RGT_NONE		((rights_bits) 0x00)	/* for std_info */
#define RUN_RGT_CREATE		((rights_bits) 0x01)	/* on super cap */
#define RUN_RGT_FINDHOST	((rights_bits) 0x02)	/* on pool cap */
#define RUN_RGT_STATUS		((rights_bits) 0x04)	/* pool/super cap */
#define RUN_RGT_DESTROY		((rights_bits) 0x08)	/* on pool cap */
#define RUN_RGT_ADMIN		((rights_bits) 0x80)	/* on pool cap */
#define RUN_RGT_ALL		((rights_bits) 0xFF)

#endif
