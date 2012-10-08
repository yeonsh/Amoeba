/*	@(#)protostack.c	1.3	94/04/06 08:51:29 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <string.h>
#include "amoeba.h"
#include "sys/flip/packet.h"
#ifdef UNIX
#include <stdio.h>
#endif
#include "assert.h"
INIT_ASSERT

/* Protocol stack functions. Most of them went to packet.h for performance
 * reasons.
 */

char *
func_proto_add_header(pkt, hsize, halign)
pkt_p pkt;
unsigned hsize, halign;
{
	assert(pkt->p_admin.pa_allocated);
	assert(pkt->p_admin.pa_header.ha_type == HAT_ABSENT);
	pkt->p_contents.pc_totsize += hsize;
	pkt->p_contents.pc_dirsize += hsize;
	pkt->p_contents.pc_offset -= hsize;
	assert(pkt->p_contents.pc_offset >= 0);
	pkt->p_admin.pa_header.ha_size = hsize;
	if (aligned(pkt_offset(pkt), halign)) {
		/* The easy and hopefully common case */
		pkt->p_admin.pa_header.ha_type = HAT_INPLACE;
		pkt->p_admin.pa_header.ha_curheader = pkt_offset(pkt);
	} else {
#ifndef SMALL_KERNEL
		printf("proto_add_header: pkt is not aligned\n");
#endif
		assert(aligned(pkt->p_contents.pc_buffer, halign));
		pkt->p_admin.pa_header.ha_type = HAT_OUTTHERE;
		pkt->p_admin.pa_header.ha_curheader = pkt->p_contents.pc_buffer;
	}
	return pkt->p_admin.pa_header.ha_curheader;
}

char *
func_proto_look_header(pkt, hsize, halign)
pkt_p pkt;
unsigned hsize, halign;
{
	assert(pkt->p_admin.pa_allocated);
	assert(pkt->p_admin.pa_header.ha_type == HAT_ABSENT);
	if (pkt->p_contents.pc_totsize < hsize)
		return 0;	/* Packet is wrong */
	if (pkt->p_contents.pc_dirsize < hsize)
		pkt_pullup(pkt, (int) hsize);
	pkt->p_admin.pa_header.ha_size = hsize;
	if (aligned(pkt_offset(pkt), halign)) {
		/* The easy and hopefully common case */
		pkt->p_admin.pa_header.ha_type = HAT_INPLACE;
		pkt->p_admin.pa_header.ha_curheader = pkt_offset(pkt);
	} else {
#ifndef SMALL_KERNEL
		printf("not aligned; copy the header\n");
#endif
		assert(aligned(pkt->p_contents.pc_buffer, halign));
		assert((pkt)->p_contents.pc_buffer + hsize < pkt_offset(pkt));
		(void) memmove((_VOIDSTAR) pkt->p_contents.pc_buffer,
			       (_VOIDSTAR) pkt_offset(pkt), (size_t) hsize);
		pkt->p_admin.pa_header.ha_type = HAT_OUTTHERE;
		pkt->p_admin.pa_header.ha_curheader = pkt->p_contents.pc_buffer;
	}
	return pkt->p_admin.pa_header.ha_curheader;
}

#ifdef ROMKERNEL
void func_proto_init(pkt)
pkt_p pkt;
{

	macro_proto_init(pkt);
}

void func_proto_fix_header(pkt)
pkt_p pkt;
{

	macro_proto_fix_header(pkt);
}

void func_proto_remove_header(pkt)
pkt_p pkt;
{

	macro_proto_remove_header(pkt);
}
#endif
