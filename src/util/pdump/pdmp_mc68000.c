/*	@(#)pdmp_mc68000.c	1.3	96/02/27 13:10:17 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
** mc68000_dumpfault - Dump a fault frame from a 68000.
**
**	NB.  the byteorder.h is for the machine running this routine and not
**	     the architecture being dumped!!
*/

#include "amoeba.h"
#include "server/process/proc.h"
#include "mc68000/fault.h"
#include "byteorder.h"

#include "stdio.h"

#include "pdump.h"

static char *regname[] = {
	"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7",
	"a0", "a1", "a2", "a3", "a4", "a5", "a6", "sp",
};

static char *space[] = {
	"0",
	"user data",
	"user program",
	"control space",
	"4",
	"kernel data",
	"kernel program",
	"cpu space"
};


static char adr[] = "   address          = %08lx\n";
static char ref[] = "   reference        = %s %s\n";
static char *error[] = { "0", };
#define NREASON (sizeof(error)/sizeof(error[0]))


/*
** stackdump - do a stacktrace.
*/

static void
stackdump(pc, fp) 
long pc;
long fp;
{
    register i;
    long nfp;
    long nargs,popinstr;
    char *string;
    long tmp;

    nfp = fp;
    do {
	fp = nfp;
	fprintf(stdout,"%8.8x: ", fp);
	fprintf(stdout,"%8.8x(", pc);
	if (!getlong_be((unsigned long) fp, &nfp) ||
	    !getlong_be((unsigned long) (fp+4), &pc) ||	/* return address */
	    !getlong_be((unsigned long) pc, &popinstr)) {
		    fprintf(stdout, "??\?)\n");
		    return;
	}
	switch(popinstr&0xFFFF0000) {
	case 0x584F0000:	/* addqw #4, sp */
	case 0x588F0000:	/* addql #4, sp */
		nargs = 1;
		break;
	case 0x504F0000:	/* addqw #8, sp */
	case 0x508F0000:	/* addql #8, sp */
		nargs = 2;
		break;
	case 0x4FEF0000:	/* lea X(sp), sp */
		nargs = (popinstr&0xFFFF)>>2;
		break;
	default:
		nargs = 0;
		break;
	}
	if (nargs>20) {
	    nargs = 20;
	    string = ".... )";
	} else {
	    string = ")";
	}

	for(i=1; i<=nargs; i++) {
	    (void) getlong_be((unsigned long) (fp+4*(i+1)), &tmp);
	    fprintf(stdout, "%x%s",
	    		tmp, i==nargs ? "" : ", ");
	}
	fprintf(stdout, "%s\n", string);
    } while (fp < nfp);
    return;
}


void
mc68000_dumpfault(tu, inlen)
    thread_ustate *tu;
    int inlen;
{
    int reason;
    int i,j;
    struct fault *in;
    struct flfault *fl;

    if (inlen <USTATE_SIZE_MIN || inlen >USTATE_SIZE_MAX) {
	fprintf(stdout, "\tWrong length fault frame, len=%d\n", inlen);
	return;
    }
    in = USTATE_FAULT_FRAME(tu);
    fl = &tu->m68us_flfault;
    reason = -1;
    if (reason != -1)
	fprintf(stdout, "\ttrap\t%8d\t%s\n", reason,
	    (reason >= 0 && reason < NREASON)? error[reason]:"???");
/* to avoid byte order problems we decode from big endian to local endian */
    DEC_INT16_BE(&in->f_sr);
    DEC_INT16_BE(&in->f_pc_high);
    DEC_INT16_BE(&in->f_pc_low);
    DEC_INT16_BE(&in->f_offset);

    fprintf(stdout,"   status register  =     %04x\n", in->f_sr);
    fprintf(stdout,"   program counter  = %08lx\n", F_PC(in));
    fprintf(stdout,"   format/offset    =     %4x\n", in->f_offset);
    switch (Format(in)) {
    case 0x0:
    case 0x1:
	break;
    case 0x2:
    case 0x9:
	dec_l_be(&in->f_f2.f2_iaddr);
	fprintf(stdout, "   instruction address  = %08lx\n", in->f_f2.f2_iaddr);
	break;
    case 0x8:
	dec_l_be(&in->f_f8.f8_faddr);
	DEC_INT16_BE(&in->f_f8.f8_ssw);
	fprintf(stdout, adr, in->f_f8.f8_faddr);
	fprintf(stdout, ref, in->f_f8.f8_ssw & F8_RW ? "read" : "write",
					space[in->f_f8.f8_ssw & FUNCODE]);
	break;
    case 0xA:
    case 0xB:
	dec_l_be(&in->f_fA.fA_faddr);
	DEC_INT16_BE(&in->f_fA.fA_ssw);
	fprintf(stdout, adr, in->f_fA.fA_faddr);
	fprintf(stdout, ref, in->f_fA.fA_ssw & FA_RW ? "read" : "write",
					space[in->f_fA.fA_ssw & FUNCODE]);
	break;
#ifdef STACKFRAME_68000
    case 0xF:
	dec_l_be(&in->f_fF.fF_faddr);
	DEC_INT16_BE(&in->f_fF.fF_ssw);
	fprintf(stdout, adr, in->f_fF.fF_faddr);
	fprintf(stdout, ref, in->f_fF.fF_ssw & FF_RW ? "read" : "write",
					space[in->f_fF.fF_ssw & FUNCODE]);
	break;
#endif /* STACKFRAME_68000 */
    default:
	fprintf(stdout,"bad format ???\n");
    }
    for (i = 0; i < 16;) {
	for (j = 0; j < 4; i++, j++)
	{
	    dec_l_be(&in->f_regs[i]);	/* go from be to local endian */
	    fprintf(stdout,"    %s = %08lx", regname[i], in->f_regs[i]);
	}
	fprintf(stdout,"\n");
    }
    if (fl->flf_flag) {
	fprintf(stdout, "   Floating point status present\n");
	/* More decoding as needed later */
    }
    stackdump((long) F_PC(in), in->f_regs[14]);
}
