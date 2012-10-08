/*	@(#)fifo.cls	1.2	94/04/06 15:49:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <amoeba.h>
#include <cmdreg.h>
#include <class/std.cls>
#include <class/stdrights.cls>

/* FIXME: The next two defines cannot be included from <h/file.h> because
 * that also contains non-ail code. 
 */
#define FSQ_READ        (TTY_FIRST_COM + 52)
#define FSQ_WRITE       (TTY_FIRST_COM + 53)

#define MAX_DATA_LEN	30000

/* We have to enforce a FSQ interface that is compatible with the
 * current (hand-written) client stubs.
 */
class fsq [ FSQ_READ .. FSQ_WRITE ] {
	inherit bullet_rgts;

	fsq_read [ FSQ_READ ] (*,
		in: h_offset	long position,
		in: h_size	int size,
		out		char data[result: MAX_DATA_LEN],
		out: h_extra	int moredata,
		out: h_size	int result
	) rights BS_RGT_READ;

	fsq_write [ FSQ_WRITE ] (*,
		in: h_offset	long position,
		in: h_size	int size,
		in		char data[size : MAX_DATA_LEN],
		out: h_size	int written
	) rights BS_RGT_MODIFY;
	
};

class fifo [FIFO_FIRST_COM .. FIFO_LAST_COM] {
	inherit bullet_rgts;

	fifo_create(*, out capability fifo_cap)	 rights BS_RGT_CREATE;

	fifo_open(*, in int flags, out capability fifo_handle)
						 rights BS_RGT_READ; /*modify?*/

	fifo_share(*)				 rights BS_RGT_MODIFY;

	fifo_close(*)				 rights BS_RGT_MODIFY;

	fifo_bufsize(*, out long size)		 rights BS_RGT_READ;

};

class fifo_svr {
	inherit stdinfo, stddestroy, stdstatus,
		stdage, stdrestrict, stdtouch, fsq, fifo;
};
