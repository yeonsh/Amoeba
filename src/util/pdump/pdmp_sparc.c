/*	@(#)pdmp_sparc.c	1.5	96/02/27 13:10:27 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * sparc_dumpfault -- dump the fault frame from a sparc process
 *
 * NB. the byteorder.h is for the machine running this routine and not
 *     the architecture being dumped!!
 */

#include "stdio.h"

#include "amoeba.h"
#include "server/process/proc.h"
#include "sparc/fault.h"
#include "sparc/interrupt.h"
#include "sparc/psr.h"
#include "byteorder.h"

#include "pdump.h"
#define SIZEOF( x ) ( sizeof( x ) / sizeof( x[ 0 ] ))

struct mapping {
    int		mp_const;
    char	*mp_name;
};

struct mapping trap_names[] = {
    { TRAPV_RESET, "reset" },
    { TRAPV_INSN_ACCESS, "instruction_access_exception" },
    { TRAPV_ILLEGAL_INSN, "illegal_instruction" },
    { TRAPV_PRIV_INSN, "priviledged_instruction" },
    { TRAPV_FP_DISB, "fp_disabled" },
    { TRAPV_WND_OVER, "window_overflow" },
    { TRAPV_WND_UNDER, "window_underflow" },
    { TRAPV_ALIGNMENT, "mem_address_not_aligned" },
    { TRAPV_FP_EXPT, "fp_exception" },
    { TRAPV_DATA_ACCESS, "data_access_exception" },
    { TRAPV_TAG_OVER, "tag_overflow" },
    { INT_IRQ1, "interrupt irq1" },
    { INT_SOFT1, "interrupt soft1" },
    { INT_IRQ2, "interrupt irq2" },
    { INT_SCSI, "interrupt scsi" },
    { INT_DVMA, "interrupt dvma" },
    { INT_IRQ3, "interrupt irq3" },
    { INT_SOFT4, "interrupt soft4" },
    { INT_LANCE, "interrupt lance" },
    { INT_IRQ4, "interrupt irq4" },
    { INT_SOFT6, "interrupt soft6" },
    { INT_VIDEO, "interrupt video" },
    { INT_IRQ5, "interrupt irq5" },
    { INTERRUPT( 8 ), "interrupt unused" },
    { INT_IRQ6, "interrupt irq6" },
    { INT_CNTR0, "interrupt cntr0" },
    { INT_FLOPPY, "interrupt floppy" },
    { INT_SERIAL, "interrupt serial" },
    { INT_IRQ7, "interrupt irq7" },
    { INT_CNTR1, "interrupt cntr1" },
    { INT_MEMORY, "interrupt memory" },
    { TRAPV_CP_DISB, "cp_disabled" },
    { TRAPV_CP_EXPT, "cp_exception" },
    { TRAPV_DIVZERO, "division by zero" },
    { TRAPV_SYSCALL, "system call" },
    { TRAPV_BPTFLT, "breakpoint" },
    { 0 }
};

struct mapping psr_names[] = {
    { PSR_C,	"c" },
    { PSR_V,	"v" },
    { PSR_Z,	"z" },
    { PSR_N,	"n" },
    { PSR_EC,	"EC" },
    { PSR_EF,	"EF" },
    { PSR_S,	"S" },
    { PSR_PS,	"PS" },
    { PSR_ET,	"ET" },
    { 0 }
};

char *find_mapping( mp, what )
struct mapping *mp;
int what;
{
    for ( ; mp->mp_name; mp++ )
	if ( mp->mp_const == what )
	    return( mp->mp_name );
    return( 0 );
}

void sparc_dumpfault( tup, tuplen )
thread_ustate *tup;
int tuplen;
{
    int i, j, bit;
    char *cp;
    struct psr psr;			/* Our psr fields */
    struct stack_frame sf;		/* Our stack frame */
    long uvsp;				/* User virtual stack pointer */
    struct callinsn {			/* Fields of the call insn */
	unsigned int	ci_op: 2;
	unsigned int	ci_disp: 30;
    } ci;
    
    printf( "sparc thread state at %x\n", tup );
    if ( tuplen != sizeof( thread_ustate )) {
	printf( "invalid size for thread_ustate (%x!=%x)\n",
	       tuplen, sizeof( thread_ustate ));
	return;
    }

    /* Decode everything in sight before we use it */
    for (i = 0; i < ARCH_NUMREGS; i++) {
	DEC_INT32_BE(&tup->tu_sf.sf_local[i]);
	DEC_INT32_BE(&tup->tu_sf.sf_in[i]);
	DEC_INT32_BE(&tup->tu_global[i]);
    }
    DEC_INT32_BE(&tup->tu_sf.sf_aggregate);
    DEC_INT32_BE(&tup->tu_sf.sf_addarg[0]);
    DEC_INT32_BE(&tup->tu_sf.sf_addarg[1]);
    DEC_INT32_BE(&tup->tu_fsr);
    DEC_INT32_BE(&tup->tu_qsize);
    for (i = 0; i < ARCH_MAXREGARGS; i++) {
	DEC_INT32_BE(&tup->tu_sf.sf_regsave[i]);
    }
    for (i = 0; i < ARCH_NUMFPREGS; i++) {
	DEC_INT32_BE(&tup->tu_float[i]);
    }
    for (i = 0; i < ARCH_FPQSIZE; i++) {
	DEC_INT32_BE(&tup->tu_fqueue[i]);
    }

    /* Now we try to dump useful information */
    i = ( tup->tu_tbr & TBR_MASK ) >> TBR_SHIFT;
    cp = find_mapping( trap_names, i );
    if ( cp == 0 ) cp = "unknown";
    printf( "Trap ``%s'' (%x) occurred at address PC=%x, NPC=%x\n",
	   cp, i, tup->tu_pc, tup->tu_npc );
    if (tup->tu_global[ 0 ] != 0) {
	/* HACK: %g0 contains fault address */
	printf( "Fault address=%x\n", tup->tu_global[ 0 ]);
    }
    psr = * (struct psr *) &tup->tu_psr;
    printf( "PID %x, %%y= %x, %%psr= %x (cwp= %x, pil= %x, ",
	   tup->tu_pid, tup->tu_y, tup->tu_psr, psr.psr_cwp, psr.psr_pil );
    for ( i = 1<<30; i != 0; i >>= 1 ) {
	if ( !( tup->tu_psr & i )) continue;
	cp = find_mapping( psr_names, i );
	if ( cp != 0 ) printf( "-%s", cp );
    }
    printf( "-)\n" );

    printf( "    globals ( 00000000 %08x %08x %08x %08x %08x %08x %08x )\n",
	   tup->tu_global[ 1 ],
	   tup->tu_global[ 2 ], tup->tu_global[ 3 ], tup->tu_global[ 4 ],
	   tup->tu_global[ 5 ], tup->tu_global[ 6 ], tup->tu_global[ 7 ]);

    printf( "       outs ( %08x %08x %08x %08x %08x %08x %08x %08x )\n",
	   tup->tu_in[ 0 ], tup->tu_in[ 1 ],
	   tup->tu_in[ 2 ], tup->tu_in[ 3 ], tup->tu_in[ 4 ],
	   tup->tu_in[ 5 ], tup->tu_in[ 6 ], tup->tu_in[ 7 ]);

    if ( psr.psr_ef ) {
	printf( "fsr = %x; qsize = %x\n\n  ", tup->tu_fsr, tup->tu_qsize );
	for ( i = 0; i < ARCH_NUMFPREGS; ) {
	    printf( "fpreg[%2d] = %08x  ", i, tup->tu_float[ i ] );
	    if ((++i % 3) == 0) printf( "\n  " );
	}
	printf( "\n\n\tFloating Point Queue\n");
	if (tup->tu_qsize > 0) {
	    printf( "\tAddress   Instruction\n" );
	    for ( i = 0; i < tup->tu_qsize * 2; i += 2) {
		printf( "[%2d]  0x%08x  0x%08x\n",
		    i, tup->tu_fqueue[ i ], tup->tu_fqueue[ i+1 ]);
	    }
	}
	else
	    printf("   Empty\n");
	printf( "\n" );
    }

    i = 0;
    uvsp = tup->tu_fp;

    for ( ;; ) {
	if ( uvsp & 3 ) {
	    printf( "stack not aligned: %08x\n", uvsp );
	    break;
	}

	if ( getstruct( uvsp, (long *) &sf, sizeof( sf ), 1 ) == 0 ) {
	    cp = "getstruct failed";
	    break;
	}

	printf( "SP %08x::\n", uvsp );
	if ( sf.sf_retaddr != 0 && sf.sf_retaddr != -1 ) {
	    if ( getlong_be( sf.sf_retaddr, (long *) &ci ) == 0 ) {
		printf( "(memory error fetching return addr insn)\n" );
	    } else {
		if ( ci.ci_op == 1 ) {
		    printf( "CALL %08x from %08x\n",
			   sf.sf_retaddr + ( ci.ci_disp << 2 ),
			   sf.sf_retaddr );
		} else {
		    printf( "XX (%08x) from %08x\n", ci, sf.sf_retaddr );
		}
	    }
	} else
	    printf( "XXXX (%08x) (dead)\n", sf.sf_retaddr );

	printf( "%2d w locals ( %08x %08x %08x %08x %08x %08x %08x %08x )\n",
	       i++, sf.sf_local[ 0 ], sf.sf_local[ 1 ],
	       sf.sf_local[ 2 ], sf.sf_local[ 3 ], sf.sf_local[ 4 ],
	       sf.sf_local[ 5 ], sf.sf_local[ 6 ], sf.sf_local[ 7 ]);
	printf( "      ins   ( %08x %08x %08x %08x %08x %08x %08x %08x )\n",
	       sf.sf_in[ 0 ], sf.sf_in[ 1 ],
	       sf.sf_in[ 2 ], sf.sf_in[ 3 ], sf.sf_in[ 4 ],
	       sf.sf_in[ 5 ], sf.sf_in[ 6 ], sf.sf_in[ 7 ]);
	if ( uvsp == sf.sf_fp ) {
	    cp = "circular stack";
	    break;
	}

	uvsp = sf.sf_fp;

	if ( sf.sf_retaddr == 0 || sf.sf_retaddr == -1 ) {
	    cp = "end of ret chain";
	    break;
	}
    }
    printf( "done: %s: %08x\n", cp, uvsp );
}
