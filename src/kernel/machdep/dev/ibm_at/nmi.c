/*	@(#)nmi.c	1.5	94/04/06 09:22:15 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * nmi.c
 *
 * In the near future I'm going to do some very clever things here:
 * like checking whether it is really a memory parity error, and if
 * so what address is causing it. Until that time, just dump the registers.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <bool.h>
#include <stddef.h>
#include <amoeba.h>
#include <exception.h>
#include <arch_proto.h>
#include <fault.h>
#include <global.h>
#include <i80386.h>
#include <machdep.h>
#include <sys/proto.h>
#include "i386_proto.h"

/*
 * NMI interrupt: on ISA/MCA machines this means a memory parity error
 */
void
nmitrap(reason, frame)
    int reason;
    struct fault *frame;
{
    unsigned long ss, esp;

    printf("\nTRAP %d: Memory parity error\n", reason);
    frame->if_cs &= 0xFFFF;
    frame->if_ds &= 0xFFFF;
    frame->if_es &= 0xFFFF;
    frame->if_fs &= 0xFFFF;
    frame->if_gs &= 0xFFFF;
    if (frame->if_cs == K_CS_SELECTOR) {
	/* no privilege transition, make up our own ss/esp */
	ss = frame->if_ds;
	esp = frame->if_faddr;
    } else {
	frame->if_ss &= 0xFFFF;
	ss = frame->if_ss;
	esp = frame->if_esp;
    }
    framedump(frame, ss, esp);
}
