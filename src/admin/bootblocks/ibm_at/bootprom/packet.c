/*	@(#)packet.c	1.1	92/06/25 14:49:19 */
/*
 * packet.c
 *
 * Packet allocation and deallocation routines.
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */
#include "assert.h"
#include "param.h"
#include "types.h"
#include "packet.h"

static packet_t *pool = (packet_t *)0;
static packet_t *last;

extern char *aalloc();

void
pkt_init()
{
    if (pool == (packet_t *)0)
	pool = (packet_t *) aalloc(PKT_POOLSIZE * sizeof(packet_t), 0);
    bzero(pool, PKT_POOLSIZE * sizeof(packet_t));
    last = pool;
}

packet_t *
pkt_alloc(offset)
    int offset;
{
    register int i;

    for (i = 0; i < PKT_POOLSIZE; i++) {
	if (last->pkt_used == FALSE) {
	    bzero(last->pkt_data, PKT_DATASIZE);
	    last->pkt_used = TRUE;
	    last->pkt_len = 0;
	    last->pkt_offset = last->pkt_data + offset;
	    return last;
	}
	if (++last == &pool[PKT_POOLSIZE])
	    last = pool;
    }
    printf("Pool out of free packets\n");
    halt();
}

void
pkt_release(pkt)
    packet_t *pkt;
{
    assert(pkt >= &pool[0]);
    assert(pkt < &pool[PKT_POOLSIZE]);
    (last = pkt)->pkt_used = FALSE;
}
