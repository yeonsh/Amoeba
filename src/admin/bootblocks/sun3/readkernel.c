/*	@(#)readkernel.c	1.3	94/04/06 11:36:18 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include <mon/sunromvec.h>
#include <stand/saio.h>
#include "kbootdir.h"

#define PRINTF	(*romp->v_printf)
#define EXIT	(*romp->v_exit_to_mon)

#define LOADADDR	((char * ) 0x80000)		/* half a meg */
#define SECSIZE		512

atoi(s)
register char *s;
{
	register sum;

	sum = 0;
	while (*s>='0' && *s <='9')
		sum = 10*sum + *s++ - '0';
	return sum;
}

bmove(p1, p2, n)
register char *p1,*p2;
{

	while (n--)
		*p2++ = *p1++;
}

bzero(p, n)
register char *p;
{
	
	while (n--)
		*p++ = 0;
}

strcmp(s1, s2)
register char *s1,*s2;
{

	while (*s1 == *s2) {
		if (*s1==0)
			return 0;
		s1++; s2++;
	}
	return *s1-*s2;
}

usage() {

	PRINTF("Amoeba bootblock for Sun 3 options:\n");
	PRINTF(">b                    boot first kernel from standard kerneldir\n");
	PRINTF(">b xx()secno          use secno instead of default for kerneldir\n");
	PRINTF(">b xx()0 kernel       boot kernel instead of first\n");
	PRINTF(">b xx()0 secno size   boot kernel from secno length size\n");
	EXIT();
}

char rdbuf[SECSIZE];
struct kerneldir kd;

struct bootparam *bp;
struct saioreq saio;

readblock(sec, ma)
long sec;
char *ma;
{

	saio.si_bn = sec;
	saio.si_cc = SECSIZE;
	saio.si_ma = ma;
	if (saio.si_cc != (*bp->bp_boottab->b_strategy)(&saio, READ)) {
		PRINTF("read kerneldir failed\n");
		EXIT();
	}
}

char *
read_kernel(vec)
long *vec;
{
	register int entry;
	register int kdsector;
	register char *memaddr;
	register int cnt,block;
	extern char edata,end;

	bzero(&edata, &end-&edata);	/* clear bss */
	bp = *romp->v_bootparam;
	saio.si_boottab = bp->bp_boottab;
	saio.si_ctlr = bp->bp_ctlr;
	saio.si_unit = bp->bp_unit;
	if ((*bp->bp_boottab->b_probe)(&saio) != 0) {
		PRINTF("Probe of disk failed\n");
		EXIT();
	}
	if (bp->bp_argv[1] && bp->bp_argv[2]) {
		block = atoi(bp->bp_argv[1]);
		cnt = atoi(bp->bp_argv[2]);
		PRINTF("Reading kernel from sector %d, size %d sectors...",
							block, cnt);
	} else {
		kdsector = atoi(bp->bp_name);
		if (kdsector == 0) {
			kdsector = vec[0];
			PRINTF("Using default kernel directory at sector %d\n",
								kdsector);
		}
		cnt = 0;
		readblock(kdsector, rdbuf);
		bmove(rdbuf, (char *) &kd, sizeof(kd));
		if (kd.kd_magic != KD_MAGIC) {
			PRINTF("kerneldir bad magic number\n");
			usage();
		}
		for (entry=0; entry<KD_NENTRIES; entry++) {
		    if (kd.kd_entry[entry].kde_size!=0) {
			if (bp->bp_argv[1]==0 ||
				strcmp(kd.kd_entry[entry].kde_name,
						bp->bp_argv[1])==0) {
			    PRINTF("Booting kernel '%s' ...",
					    kd.kd_entry[entry].kde_name);
			    block = kd.kd_entry[entry].kde_secnum+kdsector;
			    cnt = kd.kd_entry[entry].kde_size;
			    break;
			} else
			    PRINTF("Skipping kernel '%s'\n",
					    kd.kd_entry[entry].kde_name);
		    }
		}
		if (cnt == 0) {
		    PRINTF("Kernel '%s' not found\n", bp->bp_argv[1]);
		    usage();
		}
	}
	memaddr = LOADADDR;
	while (cnt) {
		readblock(block, memaddr);
		block++;
		cnt--;
		memaddr += SECSIZE;
	}
	PRINTF("\n");
	return LOADADDR;
}
