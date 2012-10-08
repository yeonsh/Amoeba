/*	@(#)st.h	1.1	96/02/27 13:50:09 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Prototypes for the SCSI disk service functions.
 *
 * Author: Gregory J. Sharp, Jan 1995
 */

#ifndef	__GENERIC_ST_H__
#define	__GENERIC_ST_H__

#include "tape/tape.h"
#include "scsicommon.h"


errstat st_unit_init _ARGS(( long devaddr, int unit ));
errstat st_read _ARGS(( long devaddr, int unit, bufsize * sz, bufptr p ));
errstat st_write _ARGS(( long devaddr, int unit, bufsize * sz, bufptr p ));
errstat st_write_eof _ARGS(( long devaddr, int unit ));
errstat st_file_skip _ARGS(( long devaddr, int unit, int32 count ));
errstat st_record_skip _ARGS(( long devaddr, int unit, int32 count));
errstat st_rewind _ARGS(( long devaddr, int unit, int imm ));
errstat st_erase_tape _ARGS(( long devaddr, int unit ));
errstat st_load _ARGS(( long devaddr, int unit ));
errstat st_unload _ARGS(( long devaddr, int unit ));
errstat st_curr_file_pos _ARGS(( long devaddr, int unit, int32 * pos ));
errstat st_curr_rec_pos _ARGS(( long devaddr, int unit, int32 * pos ));

#endif	/* __GENERIC_ST_H__ */
