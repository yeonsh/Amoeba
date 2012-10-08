/*	@(#)tapeconf.c	1.3	94/04/05 16:47:54 */
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
#include "tape/conf.h"

extern  errstat scsi_ctlr_init();
extern	errstat st_unit_init();
extern  errstat scsi_info();
extern	errstat scsi_status();
extern  errstat st_read();
extern  errstat st_write();
extern	errstat st_write_eof();
extern  errstat st_file_skip();
extern	errstat st_record_skip();
extern  errstat st_rewind();
extern  errstat st_erase_tape();
extern	errstat st_load();
extern	errstat st_unload();
extern	errstat st_curr_file_pos();
extern	errstat st_curr_rec_pos();

tape_contr_tab_type tape_contr_tab[] = {
	{ 
		"scsi", 
		0,
		32, 33,
		26,
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
