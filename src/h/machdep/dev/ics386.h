/*	@(#)ics386.h	1.2	94/04/06 16:44:26 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** InterConnect Space for the Multibus 2, i80386 board.
**
** Peter Bosch, 121190, peterb@cwi.nl
**
*/

/* Header descriptor */
#include "ics_header.h"

/* iPSB Control Record */
#define ICS_SLOT_ID		0x2D

/* PSB memory record */
#define	ICS_PSB_START_LO	0x35
#define	ICS_PSB_START_HI	0x36
#define	ICS_PSB_END_LO		0x37
#define	ICS_PSB_END_HI		0x38
#define	ICS_PSB_MEM_CTL		0x39

/* Serial Communications Record */
#define ICS_SERIAL_IN		0x6C
#define ICS_SERIAL_OUT		0x6D
#define ICS_SERIAL_STAT		0x6E
#define ICS_SERIAL_IE		0x6F
#define ICS_SERIAL_OPTIONS	0x70

