/*	@(#)pktprt.c	1.3	94/04/06 08:49:05 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef SMALL_KERNEL

#include "amoeba.h"
#include "sys/flip/packet.h"
#include "sys/flip/flip.h"
#include "assert.h"
INIT_ASSERT
#ifdef UNIX
#include <stdio.h>
#endif


adr_print(adr)
adr_p adr;
{
    register i;

    for (i = 0; i < sizeof(adr->a_abytes); i++)
        printf("%x:", adr->a_abytes[i] & 0xFF);
    printf("%x", adr->a_space & 0xFF);
}


pkt_print(pkt)
    pkt_p pkt;
{
    flip_hdr *fh;
    
    proto_cur_header(fh, pkt, flip_hdr);
    assert(fh);
    if (fh->fh_version != 1)
	printf("version: %d\n", fh->fh_version);
    switch (fh->fh_type) {
    case FL_T_LOCATE:	printf("locate ");	break;
    case FL_T_HEREIS:	printf("hereis ");	break;
    case FL_T_UNIDATA:	printf("unidata ");	break;
    case FL_T_MULTIDATA:	printf("multidata ");	break;
    case FL_T_NOTHERE:	printf("nothere ");	break;
    case FL_T_UNTRUSTED: printf("untrusted "); 	break;
    default:		printf("%d", fh->fh_type);
    }
    adr_print(&fh->fh_dstaddr);
    printf(" ");
    adr_print(&fh->fh_srcaddr);
    printf(" f %x ", fh->fh_flags & 0xFF);
    printf("id %d ", fh->fh_messid);
    printf("a %d ", fh->fh_act_hop);
    printf("m %d ", fh->fh_max_hop);
    printf("l %d ", fh->fh_length);
    printf("o %d ", fh->fh_offset);
    printf("t %d\n", fh->fh_total);
}
#endif /* SMALL_KERNEL */
