/*	@(#)tapeconf.c	1.1	96/03/26 11:49:37 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
**	Tape table
**
**	This file contains the tape configuration information for the tapes
**	and tape controllers attached to the machine.  At present it is
**	rather complicated but some day we will try to generate it from a
**	table.
*/

#include "amoeba.h"
#include "interrupt.h"
#include "tape/conf.h"
#include "st.h"		/* prototypes for SCSI tape interface */


tape_contr_tab_type tape_contr_tab[] = {
	{ 
		"scsi", 
		0,
		32, 33,
		INT_SCSI,
		scsi_ctlr_init,
		st_unit_init,
		scsi_info,
		scsi_status,
		st_read,
		st_write,
		st_write_eof,
		st_file_skip,
		st_record_skip,
		st_rewind,
		st_erase_tape,
		st_load,
		st_unload,
		st_curr_file_pos,
		st_curr_rec_pos,
	}
};

int num_tape_ctlrs = sizeof(tape_contr_tab)/sizeof(tape_contr_tab_type);
