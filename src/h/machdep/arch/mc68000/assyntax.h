/*	@(#)assyntax.h	1.4	94/04/06 16:00:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __ASSYNTAX_H__
#define __ASSYNTAX_H__

/*
 * An attempt to hide the difference between Motorola syntax and PDP 11 style
 * syntax as used by the ACK assembler
 */


#ifdef MOTOROLA

#define COM		|
#define CHOICE(a,b,c,d)	c
#define AS_BEGIN

#else /* MOTOROLA */

#ifdef	ACK_ASSEMBLER
#define COM		!
#define CHOICE(a,b,c,d)	a
#define AS_BEGIN	.sect .text;.sect .rom;.sect .data;.sect .bss;.sect .text
#else /* ACK_ASSEMBLER */

#ifdef ACE_ASSEMBLER
#define COM		|
#define CHOICE(a,b,c,d)	d
#define AS_BEGIN

#else /* ACE_ASSEMBLER */

#define COM		|
#define CHOICE(a,b,c,d)	b
#define AS_BEGIN
#endif /* ACE_ASSEMBLER */
#endif /* ACK_ASSEMBLER */
#endif /* MOTOROLA */

#define EXPR(a)		CHOICE({a}, (a), a, a)

/* Directives */
#define GLOBLDIR	CHOICE(.extern, .globl, .globl, .globl)
#define SECTBSS		CHOICE(.sect .bss, .bss, .bss, .bss)
#define SECTTEXT	CHOICE(.sect .text, .text, .text, .text)
#define SECTDATA	CHOICE(.sect .data, .data, .data, .data)
#define WORD		CHOICE(.data2, .word, .word, .word)
#define LONG		CHOICE(.data4, .long, .long, .long)
#define SKIP		CHOICE(.space, .skip, skip, .space)

/* Instructions */
#define ADDW		CHOICE(add.w, addw, add.w, add.w)
#define ADDQW		CHOICE(add.w, addqw, add.w, addq.w)
#define ADDL		CHOICE(add.l, addl, add.l, add.l)
#define ADDQL		CHOICE(add.l, addql, add.l, addq.l)
#define ADDAW		CHOICE(add.w, addqw, adda.w, adda.w)
#define ADDAL		CHOICE(add.l, addl, adda.l, adda.l)
#define ADDQAL		CHOICE(add.l, addql, adda.l, addq.l)
#define ANDL		CHOICE(and.l, andl, and.l, and.l)
#define ANDW		CHOICE(and.w, andw, and.w, and.w)
#define ASLL		CHOICE(asl.l, asll, asl.l, asl.l)
#define ASRL		CHOICE(asr.l, asrl, asr.l, asr.l)
#define CASB		CHOICE(cas.b, casb, cas.b, cas.b)
#define CASW		CHOICE(cas.w, casw, cas.w, cas.w)
#define CASL		CHOICE(cas.l, casl, cas.l, cas.l)
#define CLRB		CHOICE(clr.b, clrb, clr.b, clr.b)
#define CLRW		CHOICE(clr.w, clrw, clr.w, clr.w)
#define CLRL		CHOICE(clr.l, clrl, clr.l, clr.l)
#define CMPL		CHOICE(cmp.l, cmpl, cmp.l, cmp.l)
#define CMPW		CHOICE(cmp.w, cmpw, cmp.w, cmp.w)
#define CMPAW		CHOICE(cmp.w, cmpw, cmpa.w, cmpa.w)
#define CMPAL		CHOICE(cmp.l, cmpl, cmpa.l, cmpa.l)
#define LSRW		CHOICE(lsr.w, lsrw, lsr.w, lsr.w)
#define LSRL		CHOICE(lsr.l, lsrl, lsr.l, lsr.l)
#define MOVEML		CHOICE(movem.l, moveml, movem.l, movem.l)
#define MOVEQ		CHOICE(move.l, moveq, move.l, moveq)
#define MOVB		CHOICE(move.b, movb, move.b, move.b)
#define MOVL		CHOICE(move.l, movl, move.l, move.l)
#define MOVW		CHOICE(move.w, movw, move.w, move.w)
#define MOVAW		CHOICE(move.w, movw, movea.w, movea.w)
#define MOVAL		CHOICE(move.l, movl, movea.l, movea.l)
#define MOVESB		CHOICE(moves.b, movesb, moves.b, moves.b)
#define MOVESW		CHOICE(moves.w, movesw, moves.w, moves.w)
#define MOVESL		CHOICE(moves.l, movesl, moves.l, moves.l)
#define ORB		CHOICE(or.b, orb, or.b, or.b)
#define SUBL		CHOICE(sub.l, subl, sub.l, sub.l)
#define SUBW		CHOICE(sub.w, subw, sub.w, sub.w)
#define SUBQL		CHOICE(sub.l, subql, sub.l, subq.l)
#define SUBQW		CHOICE(sub.w, subqw, sub.w, subq.w)
#define SUBAW		CHOICE(sub.w, subqw, suba.w, suba.w)
#define SUBQAL		CHOICE(sub.l, subql, suba.l, subq.l)
#define SUBAL		CHOICE(sub.l, subl, suba.l, suba.l)
#define TSTL		CHOICE(tst.l, tstl, tst.l, tst.l)
#define TSTW		CHOICE(tst.w, tstw, tst.w, tst.w)
#define TSTB		CHOICE(tst.b, tstb, tst.b, tst.b)
#define JBSR		CHOICE(bsr, jbsr, bsr, jbsr)
#define JRA		CHOICE(bra, jra, bra, bra)
#define JEQ		CHOICE(beq, jeq, beq, jeq)
#define JNE		CHOICE(bne, jne, bne, jne)
#define JLS		CHOICE(bls, jls, bls, jls)
#define JLE		CHOICE(ble, jle, ble, jle)
#define	JLT		CHOICE(blt, jlt, blt, jlt)
#define JGE		CHOICE(bge, jge, bge, jge)
#define	JGT		CHOICE(bgt, jgt, bgt, jgt)
#define	JPL		CHOICE(bpl, jpl, bpl, jpl)
#define	JMI		CHOICE(bmi, jmi, bmi, jmi)
#define JHI		CHOICE(bhi, jhi, bhi, jhi)

/* Adressing modes */
#define GLOBAL(x)	CHOICE((x), x, x, x)
#define AUTODEC(reg)	CHOICE(-(reg), reg@-, -(reg), -(reg))
#define AUTOINC(reg)	CHOICE((reg)+, reg@+, (reg)+, (reg)+)
#define REGINDIR(reg)	CHOICE((reg),reg@, (reg), (reg))
#define REGOFFSETTED(reg, offset) CHOICE((offset,reg), reg@(offset), offset(reg), offset(reg))
#define BDAREGINDEXW(base, areg, index) CHOICE((base, areg, index.w),areg@(base,index:W), (base, areg, index.w), (base, areg, index.w))
#define IMM(addr)	CHOICE((addr), addr, addr, addr)

#endif /* __ASSYNTAX_H__ */
