/*	@(#)flrpc_kid.h	1.3	94/04/06 08:45:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#define	TSN_WINDOW	512		/* Must be power of two */

#if TSN_WINDOW&(TSN_WINDOW-1)
#include "What did I say?"
#endif

#define TSN_BITMAP_SIZE	(TSN_WINDOW/32)
#define TSN_BITMAP_MODULO (TSN_BITMAP_SIZE-1)

typedef struct kid {
	uint32 	k_last;
	uint32	k_bitmap[TSN_BITMAP_SIZE];
	int	k_index;
	adr_t   k_kid;
	long	k_idle;
	struct kid *k_next;
} kid_t, *kid_p;

#define KIDBITWORD(kid, bit) kid->k_bitmap[((bit>>5)+kid->k_index)&TSN_BITMAP_MODULO]
#define KIDBITBIT(bit) (1<<(bit&31))

#define SETBIT(kid, bit) KIDBITWORD((kid), (bit)) |= KIDBITBIT((bit))
#define GETBIT(kid, bit) (KIDBITWORD((kid), (bit)) & KIDBITBIT((bit)))


kid_p kid_install();
kid_p kid_lookup();
int kid_execute();
void kid_init();
void kid_update();
int kid_dump();
