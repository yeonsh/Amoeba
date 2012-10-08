/*	@(#)printbuf.c	1.6	96/02/27 14:22:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
printbuf.c

Created Oct 1, 1991 by Philip Homburg
*/

#include <amoeba.h>
#include <assert.h>
INIT_ASSERT
#include "map.h"
#include "machdep.h"
#include <sys/printbuf.h>
#include <sys/seg.h>

extern void seg_settype();
struct printbuf *pb_descr;

void init_printbuf()
{
	unsigned int pb_start_clicks;
	assert(!pb_descr);

	/* let's reserve the memory for the print buffer */
	pb_start_clicks = (unsigned) PB_START;
	pb_start_clicks >>= (unsigned) CLICKSHIFT;
	seg_mark_used(pb_start_clicks,
		      BSTOC((vir_bytes) PB_SIZE), (long) ST_KERNEL);
	pb_descr= (struct printbuf *)PB_START;
	pb_descr->pb_buf= (char *)&pb_descr[1];
	pb_descr->pb_end= (char *)PB_START + PB_SIZE;
	if (pb_descr->pb_next < pb_descr->pb_buf ||
	    pb_descr->pb_next >= pb_descr->pb_end)
		pb_descr->pb_next= pb_descr->pb_buf;
	pb_descr->pb_first= pb_descr->pb_next+1;
	if (pb_descr->pb_first == pb_descr->pb_end)
		pb_descr->pb_first= pb_descr->pb_buf;
}
