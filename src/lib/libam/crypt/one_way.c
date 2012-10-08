/*	@(#)one_way.c	1.3	96/02/27 11:00:03 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "module/crypt.h"

#ifndef KERNEL

#include "module/mutex.h"
static mutex lock;

#endif /* KERNEL */

#define LOGNBUCKETS	5
#define BUCKETSIZE	2

#define NBUCKETS	(1<<LOGNBUCKETS)
#define BUCKETHASH(p)	((p)->_portbytes[0]&(NBUCKETS-1))

static struct bucket {
	int bu_next;
	struct bcontents {
		port buc_in;
		port buc_out;
	} bu_contents[BUCKETSIZE];
} bucket[NBUCKETS];


#if PORTSIZE!=6
This code needs it
#endif

static void
do_oneway(p,q)
port *p,*q;
{
	char key[64];
	char block[48];
	static port nullport;
	register i,j;
	register char *cp;

	/*
	 * We actually need 64 bit key.
	 * Throw some zeroes in at bits 6 and 7 mod 8
	 * The bits at 7 mod 8 etc are not used by the algorithm
	 */
	for (i=0,j=0,cp=key; i< 64; i++)
		if ((i&7)>5)
			*cp++ = 0;
		else {
			*cp++ = (p->_portbytes[j>>3]&(1<<(j&7))) != 0;
			j++;
		}
	OWsetkey(key);
	/*
	 * Now go encrypt constant 0
	 */
	for(i=0,cp=block;i<48;i++)
		*cp++ = 0;
	OWcrypt48(block);
	/*
	 * and put the bits in the destination port
	 */
	*q = nullport;
	cp = block;
	for (i=0;i<PORTSIZE;i++)
		for (j=0;j<8;j++)
			q->_portbytes[i] |= *cp++<<j;
}

void
one_way(p,q)
port *p,*q;
{
	register struct bucket *bp;
	register struct bcontents *bcp;
	register i;

	/*
	 * null port should be left alone
	 */

	if (NULLPORT(p)) {
		*q = *p;
		return;
	}

	/*
	 * then check cache
	 */

	bp = &bucket[BUCKETHASH(p)];
	for(i=0,bcp=bp->bu_contents;i<BUCKETSIZE;i++,bcp++)
		if (PORTCMP(p,&bcp->buc_in)) {
			/*
			 * We struck lucky
			 */
			*q = bcp->buc_out;
			return;
		}
#ifndef KERNEL
	/*
	 * In the case of pre-emptive scheduling this can get into problems
	 * so we have a mutex to protect updates to the global bucket.
	 */
	mu_lock(&lock);
#endif /* KERNEL */

	bcp = &bp->bu_contents[bp->bu_next++];
	if (bp->bu_next>=BUCKETSIZE)
		bp->bu_next = 0;	/* circular list */
	bcp->buc_in = *p;
	do_oneway(&bcp->buc_in,&bcp->buc_out);
	if (NULLPORT(&bcp->buc_out))
		bcp->buc_out._portbytes[0] |= 1;
	*q = bcp->buc_out;

#ifndef KERNEL
	mu_unlock(&lock);
#endif /* KERNEL */
}
