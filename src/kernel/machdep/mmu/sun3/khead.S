/*	@(#)khead.S	1.7	94/04/06 09:42:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "assyntax.h"
#include "map.h"
AS_BEGIN

#ifndef ACK_ASSEMBLER
#define SETLED(v)	MOVL v,d2; MOVESB d2,IMM(0x70000000); MOVW #100,d4; \
			1: MOVW #-1,d3; 2: dbra d3,2b; dbra d4,1b
#else
#define SETLED(v)	MOVL #v,AUTODEC(sp); jsr GLOBAL(_setled); ADDQL #4, sp
#endif

GLOBLDIR startup, _abort

#ifdef ACK_ASSEMBLER
.define .lino,.filn,.trpim
#endif

COM The SUN monitor has set up a certain memory map, that needs to be
COM maintained to be able to return to it.  The easiest thing to do is not
COM to change the relevant parts.  Only one physical page of memory is used,
COM namely the last 8K page.  In this page the monitor maintains the interrupt
COM vectors, the stack, and the font table.  The code below 
COM uses as little of the monitor map info as possible, but it is lazy at
COM times.  For example, it uses the fact that the interrupt register is mapped
COM at FE0A000, and that the interrupt vectors are stored at FE60C00.  The
COM relevant part of the monitor map is between FE00000 and 10000000.  That
COM part is left unchanged.  The kernel map starts at F000000 (IVECBASE).
COM The interrupt vectors are (virtually) put here.  The kernel code starts
COM physically at 4000.  If the kernel is not loaded there, then it is copied
COM first.  The monitor stack pointer is saved for returning to the monitor
COM later.  The kernel is mapped from F000000 to 0 in all 8 contexts.  Note
COM that the kernel code should virtually start at F004000.  Now that the
COM kernel is safely mapped the program counter is set, execution of C code
COM (clearbss and main) begins.  Below is a short summary of the mapping
COM including the setup as is done in the C initmm routine.
COM
COM	00000000 - 08000000	Amoeba user space
COM	08000000 - 0E000000	Amoeba IO space
COM	0E000000 - 0F000000	Amoeba bitmap
COM	0F000000 - 0F004000	Amoeba vectors
COM	0F004000 - 0FE00000	Amoeba code + data
COM	0FE00000 - 0FE20000	monitor IO space
COM	0FE20000 - 0FE60000	monitor bitmap
COM	0FE60000 - 0FE60C00	monitor stack
COM	0FE60C00 - 0FE61000	monitor vectors
COM	0FE61000 - 0FE62000	monitor font table
COM	0FE62000 - 0FEF0000	monitor reserved for other uses
COM	0FEF0000 - 0FF00000	monitor PROM (vector table + code)
COM	0FF00000 - 10000000	monitor DVMA map

ldaddr:
	MOVB	#0,IMM(0xFE0A000)	COM disable nmi interrupts
	MOVW	#0x2700,sr		COM ignore interrupts
	MOVEQ	#3,d0			COM put control space in sfc & dfc
	movec	d0,sfc
	movec	d0,dfc

COM copy the code from wherever it is to addr 0x4000. a0 is where it is now.
COM but there must be a hole larger than the kernel to do it
this:
#if defined(MOTOROLA) || defined(ACE_ASSEMBLER)
	lea	REGOFFSETTED(pc,ldaddr-this-2),a0
					COM where are we currently loaded?
	lea	IMM(ldaddr-IVECBASE),a1 COM where we want to be loaded
	CMPAL	a0,a1
#else
#if defined(ACK_ASSEMBLER) || defined(GNU_ASSEMBLER)
	WORD	0x41fa;  WORD EXPR(ldaddr - EXPR(this+2))
					COM where are we currently loaded?
#else
	lea	REGOFFSETTED(pc,EXPR(ldaddr-EXPR(this+2))),a0
					COM where are we currently loaded?
#endif /* ACK_ASSEMBLER */
	lea	IMM(EXPR(ldaddr-IVECBASE)),a1 COM where we want to be loaded
	CMPAL	a0,a1
#endif /* MOTOROLA || ACE_ASSEMBLER */
	JEQ	go_on			COM already at 0x4000 so don't copy

	MOVL	#_edata,d0		COM calculate size of the segments
	SUBL	#ldaddr,d0
	LSRL	#2,d0			COM convert bytes to words
copyloop:
	MOVL	AUTOINC(a0),AUTOINC(a1)
	dbra	d0,copyloop
	SUBL	#0x10000, d0
	CMPL	#0xFFFFFFFF, d0
	JNE	copyloop
	jmp	IMM(EXPR(go_on-IVECBASE)) COM go to the code we were running before
go_on:
	MOVL	sp,IMM(EXPR(monsp-IVECBASE))	COM save sp


/*
COM mmsetup(){
COM 	for (d7 = 7; d7 != -1; d7--) {	** d7: context **
COM 		CONTEXT = d7;
COM 		for (d6 = 127 - 16; d6 != -1; d6--)	** d6: segment **
COM 			SEGMAP[d6] = d6;
COM 	}
COM 	d7 = 0xF0000800;		** d7: page map contents **
COM 	for (d6 = 0x7FF; d6 != -1; d6--)		** d6: page **
COM 		PAGEMAP[d6] = --d7;
COM }
*/

	MOVEQ	#7,d7			COM # contexts - 1
ctxloop:
	MOVESB	d7,IMM(0x30000000) COM CONTEXT = d7 (top 16 segs is monitor)
	MOVEQ	#111,d6			COM # kernel segments - 1
	MOVAL	#0x2FE00000,a5		COM top segmap
segloop:
	SUBAL	#0x20000,a5		COM a5 -= SEGSIZE
	MOVESB	d6,REGINDIR(a5)			COM SM(seg) = d6
	dbra	d6,segloop
	dbra	d7,ctxloop
	
	MOVL	#2047,d6		COM # pages - 1
	MOVAL	#0x11000000,a5		COM last page
	MOVL	#0xF0000800,d7		COM value for page map
pageloop:
	lea	REGOFFSETTED(a5,EXPR(-0x2000)),a5
					COM a5 -= PAGESIZE
	SUBQL	#1,d7
	MOVESL	d7,REGINDIR(a5)
	dbra	d6,pageloop

	MOVAL	#start, a0		COM force 32 bit address
	jmp	REGINDIR(a0)		COM set pc
start:
	SUBAL	a6,a6			COM to terminate a6-chain cleanly
	MOVAL	#_kst_end,sp
	jsr	GLOBAL(_clearbss)	COM clear uninitialized data
#ifdef STACKFRAME_68000
	CLRW	GLOBAL(_is68000)	COM at least 68010
#endif
	MOVEQ	#9,d0			COM clear & enable cacr
	movec	d0,cacr			COM ditto
	MOVL	#IVECBASE,d0
	movec	d0,vbr			COM vector base register = 0xF000000
COM Attempt at getting fpu working:
	MOVL	#0xff, d0
	MOVESB	d0, IMM(0x40000000)
startup:
	jsr	GLOBAL(_main)

_abort:
	jsr	GLOBAL(_reboot)		COM if it fails then loop
abort:
	JRA	abort


GLOBLDIR _initboard
_initboard:
	CLRB	d0
	MOVESB	d0,IMM(0x70000000)	COM light all leds
	rts

COM To return to the monitor the vector base register and the sp need to be
COM restored.  Unfortunately, we've noticed that sometimes the vbr is set to
COM zero as our code is entered, so we've hardwired the address of the monitor
COM vector base here.  The stack pointer was saved in the global variable
COM monsp, and can be restored here.  We also enable NMI interrupts because
COM the monitor uses this for watchdog interrupts.
GLOBLDIR _reboot
_reboot:
	MOVL	#0xFE60C00, d0		COM get monitor vbr
	movec	d0,vbr			COM restore
	ORB	#0x80,IMM(0x800A000)	COM enable NMI
	MOVAL	GLOBAL(monsp),sp	COM restore sp
	rts				COM return to monitor

GLOBLDIR _setled
_setled:
	MOVL	REGOFFSETTED(sp,4),d0
	MOVESB	d0,IMM(0x70000000)
	MOVW	#100,d1
 dl1:
	MOVW	#-1,d0
 dl2:
	dbra	d0,dl2
	dbra	d1,dl1
	rts


GLOBLDIR _begin, _kst_beg, _kst_end
SECTDATA
_kst_beg:	SKIP 10240
_kst_end:
monsp:		SKIP 4			COM save to return to monitor

#ifdef ACK_ASSEMBLER
.lino:
        .data4  0               COM lino
.filn:
        .data4  0               COM filn
.trpim:
        .data4  0
#endif

#ifndef GNU_ASSEMBLER
SECTBSS
#endif

/* _begin should be at the beginning of the bss. Unfortunately there is
 * no portable way to force this:
 */
#ifdef ACK_ASSEMBLER
_begin: SKIP 4
#else
.comm	_begin,4
#endif
