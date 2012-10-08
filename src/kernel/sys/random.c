/*	@(#)random.c	1.6	96/02/27 14:22:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "assert.h"
INIT_ASSERT
#include "randseed.h"
#include "module/rnd.h"
#include "sys/proto.h"

extern long random();


/*
 * Collection code for seed bits
 */

static int inited;
static unsigned long seedcollection;
static int seedbits[RANDSEED_NTYPES];

#ifndef SMALL_KERNEL
random_dump(begin, end)
char *begin;
char *end;
{
    register char *p;
    
    p = bprintf(begin, end, "=========== seed dump ==========\n");
    p = bprintf(p, end, "seed=%X, bits=(%d,%d,%d)\n", seedcollection,
	seedbits[0], seedbits[1], seedbits[2]);
    return p - begin;
}
#endif

void randseed(value, nbits, bittype)
unsigned long value;
int nbits;
int bittype;
{

	compare(nbits, >, 0);
	compare(nbits, <=, sizeof(value)*8);
	compare(bittype, >=, 0);
	compare(bittype, <, RANDSEED_NTYPES);
	assert(!inited);
	seedbits[bittype] += nbits;
	for (; nbits>0 ; nbits--) {
		seedcollection ^= value&1;
		value >>= 1;
		/* Rotate seedcollection one bit to the left */
		if (seedcollection&0x80000000)
			seedcollection = (seedcollection<<1)|1;
		else
			seedcollection = (seedcollection<<1)  ;
	}
}

void
initrand()
{
  int i;

  compare(seedbits[RANDSEED_INVOC] + seedbits[RANDSEED_RANDOM], >=, 12 );
  /*
   * Start the random() running
   */
  srandom(seedcollection);
  /*
   * To make the upper bits have some influence on the lower
   */
  for(i=seedcollection>>20; i != 0; i--)
	(void) random();
  inited++;
}

uint16 rand() {

  assert(inited);
  /*
   * It turns out that the effect of a bit in the seed only goes right
   * to left, ie more significant bits are effected, but less significant
   * bits are not. Therefore we use the upper 16 bits of random().
   */
  return random()>>15;
}

void
uniqport(p)		/* generate a random port */
port *p;
{
  register uint16 *q = (uint16 *) p;

  *q++ = rand();
  *q++ = rand();
  *q++ = rand();
}
