/*	@(#)tapeconf.c	1.3	96/02/16 15:48:48 */
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
#include "memaccess.h"
#include "machdep.h"
#include "map.h"
#include "scsi_sun3.h"
#include "tape/conf.h"
#include "generic/st.h"


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

/*
** Where to find the scsi device and its corresponding dma and control
** registers
*/
struct scsi_info        Scsi_info[SUN_NUM_SCSI_CONTROLLERS] = {
    {
        (IOBASE + 0x14000),
        (IOBASE + 0x14010),
        (IOBASE + 0x14014),
        0xF00000,
    }
};
