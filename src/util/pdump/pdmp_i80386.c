/*	@(#)pdmp_i80386.c	1.5	96/02/27 13:10:11 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * pdump_i80386.c
 *
 * Dump a process descriptor from an i80386.
 */
#include "stdio.h"
#include "pdump.h"
#include "amoeba.h"
#include "server/process/proc.h"
#include "i80386/fault.h"
#include "byteorder.h"

static char *error[] = {
    "divide error",
    "debug exception",
    "NMI",
    "break point",
    "overflow",
    "bounds check",
    "invalid opcode",
    "coprocessor not available",
    "double fault",
    "coprocessor segment overrun",
    "invalid TSS",
    "segment not present",
    "stack exception",
    "general protection",
    "page fault",
    "trap 15",
    "coprocessor error",
};

#define NREASON (sizeof(error)/sizeof(error[0]))

/*
 * Do a stack back trace
 */
static void
stackdump(eip, ebp, esp)
    long eip, ebp, esp;
{
    int i;
    long tmp, narg;
    long nebp, neip;
    int guessing;

    nebp = ebp;
    if (nebp == 0) {
	/* If ebp is 0, the thread is probably blocked in a system call.
	 * We need a valid ebp to get a traceback, however.  We can try
	 * to find it using esp: it should be close to the top of the stack.
	 * We skip the first two words (one containing _thread_local)
	 * and start looking for a frame pointer, using some guesswork.
	 */
	unsigned long addr;

	for (addr = esp + 8; addr < (unsigned long) (esp + 80); addr += 4) {
	    long try;

	    if (!getlong_le(addr, &try)) {
		fprintf(stderr, "cannot get stack word 0x%lx\n", addr);
		break;
	    } else if ((try > esp) && (try < esp + 160)) {
		/* this looks like a frame pointer */
		nebp = try;
		break;
	    }
	}
    }

    do {
	narg = 0;
	guessing = 0;
	ebp = nebp;
	fprintf(stdout, "%x: %x(", ebp, eip);
	if (!getlong_le((unsigned long) ebp, &nebp) ||
	  !getlong_le((unsigned long) (ebp+4), &eip) ||
	  !getlong_le((unsigned long) eip, &neip)) {
	    fprintf(stdout, "??\?)\n");
	    return;
	}
	if ((neip & 0xF8) == 0x58) {
	    if ((neip & 0xF800) == 0x5800)
		if ((neip & 0xF80000) == 0x580000)
		    narg = 3;
		else
		    narg = 2;	/* pop ecx; pop ecx */
	    else
		narg = 1; /* pop ecx */
	}
	else if ((neip & 0xffff) == 0xC483) {
	    /* add sp,XX (sign-extended byte) */
	    narg = ((neip & 0xFF0000) >> 16) / 4;
	} else if ((neip & 0xffff) == 0xc481) {
	    /* add sp,XX (longword) */ 
	    (void) getlong_le((unsigned long) (neip+2), &narg);
	    narg /= 4;
	}
	
	if (narg > 7) {
	    narg = 7;
	    guessing++;
	}

	for (i = 0; i < narg; i++) {
	    (void) getlong_le((unsigned long) (ebp+4*(i+2)), &tmp);
	    fprintf(stdout, "%s%x", i == 0 ? "": ", ", tmp);
	}
	if (guessing) fprintf(stdout, ", ...");
	fprintf(stdout, ")\n");
    } while (ebp < nebp);
    fprintf(stdout, "%x: called from %x\n", ebp, eip);
}


void
i80386_dumpfault(fp, inlen)
    struct fault *fp;
    int inlen;
{
    long *tfp = (long *)fp;
    int i, reason;

    if (inlen != sizeof(struct fault)) {
	fprintf(stdout, "\tUnrecognized fault frame, len=%d, wanted=%d\n",
	    inlen, sizeof(struct fault));
	return;
    }

    /* byte swap the fault frame */
    for (i = 0; i < 20; i++) dec_l_le(&tfp[i]);

    fp->if_cs &= 0xFFFF;
    fp->if_ds &= 0xFFFF;
    fp->if_es &= 0xFFFF;
    fp->if_fs &= 0xFFFF;
    fp->if_gs &= 0xFFFF;
    fp->if_ss &= 0xFFFF;

    fprintf(stdout, "\n");
    if ((reason = fp->if_trap) != -1)
	fprintf(stdout, "TRAP %d: %s\n", reason, 
	    (reason >= 0 && reason < NREASON)? error[reason]:"???");

    fprintf(stdout, "cs:eip\t\t%8x:%8x\n", (fp->if_cs & 0xFFFF), fp->if_eip); 
    if (fp->if_ss != fp->if_ds)
	fprintf(stdout, "ss:esp\t\t%8x:%8x\n", fp->if_ds, fp->if_ebp);
    else
	fprintf(stdout, "ss:esp\t\t%8x:%8x\n", fp->if_ss, fp->if_esp);
    fprintf(stdout, "eflag\t\t%8x\n", fp->if_eflag);
    fprintf(stdout, "ebp\t\t%8x\n", fp->if_ebp);
    fprintf(stdout, "ds/es/fs/gs\t%8x %8x %8x %8x\n",
	fp->if_ds, fp->if_es, fp->if_fs, fp->if_gs);
    fprintf(stdout, "eax/ebx/ecx/edx\t%8x %8x %8x %8x\n",
	fp->if_eax, fp->if_ebx, fp->if_ecx, fp->if_edx);
    fprintf(stdout, "edi/esi\t\t%8x %8x\n", fp->if_edi, fp->if_esi);
    fprintf(stdout, "cr3\t\t%8x\n", fp->if_memmap);
    fprintf(stdout, "faultaddr\t%8x\n", fp->if_faddr);
    fprintf(stdout, "code\t\t%8x\n", 
	(fp->if_trap == 13) ? (fp->if_errcode & 0xFFFF): fp->if_errcode);
    fprintf(stdout, "\n");
    stackdump((long) fp->if_eip, (long) fp->if_ebp, (long) fp->if_esp);
}
