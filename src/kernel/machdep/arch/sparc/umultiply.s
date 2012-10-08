/*	@(#)umultiply.s	1.2	94/04/11 14:18:58 */
/*
 * umul.s
 *
 * Of course, pure RISC processors don't have a unsigned multiply instruction.
 */
	.seg	"text"

	.globl .umul
.umul:
	mov	%o0, %y
	andncc	%o0, 0xf, %o4
	be	.umul1
	sethi	%hi(0xffff0000), %o5
	andncc	%o0, 0xff, %o4
	be,a	.umul3
	mulscc	%o4, %o1, %o4
	andncc	%o0, 0xfff, %o4
	be,a	.umul5
	mulscc	%o4, %o1, %o4
	andcc	%o0, %o5, %o4
	be,a	.umul7
	mulscc	%o4, %o1, %o4
 	andcc	%g0, %g0, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %g0, %o4
 	orcc	%g0, %o1, %g0
 	bge	.umul0
 	nop
 	add	%o4, %o0, %o4
.umul0:
	rd	%y, %o0
 	jmp	%o7 + 0x8
 	addcc	%o4, %g0, %o1
.umul1:
	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %g0, %o4
 	rd	%y, %o5
 	orcc	%g0, %o1, %g0
 	bge	.umul2
 	sra	%o4, 0x1c, %o1
 	add	%o1, %o0, %o1
.umul2:
	sll	%o4, 0x4, %o0
	srl	%o5, 0x1c, %o5
	or	%o5, %o0, %o0
	jmp	%o7 + 0x8
	orcc	%g0, %o1, %g0
.umul3:
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %g0, %o4
	rd	%y, %o5
	orcc	%g0, %o1, %g0
	bge	.umul4
	sra	%o4, 0x18, %o1
	add	%o1, %o0, %o1
.umul4:
	sll	%o4, 0x8, %o0
	srl	%o5, 0x18, %o5
	or	%o5, %o0, %o0
	jmp	%o7 + 0x8
	orcc	%g0, %o1, %g0
.umul5:
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %g0, %o4
	rd	%y, %o5
	orcc	%g0, %o1, %g0
	bge	.umul6
	sra	%o4, 0x14, %o1
	add	%o1, %o0, %o1
.umul6:
	sll	%o4, 0xc, %o0
	srl	%o5, 0x14, %o5
	or	%o5, %o0, %o0
	jmp	%o7 + 0x8
	orcc	%g0, %o1, %g0
.umul7:
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %g0, %o4
	rd	%y, %o5
	orcc	%g0, %o1, %g0
	bge	.umul8
	sra	%o4, 0x10, %o1
	add	%o1, %o0, %o1
.umul8:
	sll	%o4, 0x10, %o0
	srl	%o5, 0x10, %o5
	or	%o5, %o0, %o0
	jmp	%o7 + 0x8
	orcc	%g0, %o1, %g0
