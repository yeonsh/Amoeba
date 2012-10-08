/*	@(#)f30sram.c	1.6	96/02/27 13:47:49 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "sys/proto.h"
#include "stderr.h"
#include "machdep.h"
#include "sram.h"

void
f30sram_init()
{
    int i;
    long *p;
    long sum;
    
    p = F30SRAM->f30sram_boot_setup.f3s4k_long;
    sum = 0;
    for (i=0;i<256;i++)
	sum += *p++;
    printf("Sum = %x\n", sum);
}

void
f30sram_make()
{
    errstat r;
    capability cap;
    
    r = HardwareSeg(&cap, (phys_bytes) F30SRAM->f30sram_mem,
		    (phys_bytes) SRAMMEM, 0);
    if (r != STD_OK) {
	printf("f30sram_init: HardwareSeg failed: %s\n", err_why(r));
    } else {
	r = dirappend("sram_seg", &cap);
	if (r != STD_OK) {
	    printf("f30sram_init: dir_append failed: %s\n", err_why(r));
	}
    }
}
