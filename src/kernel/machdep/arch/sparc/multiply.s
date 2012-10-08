/*	@(#)multiply.s	1.2	94/04/11 14:18:40 */
/*
 * mul.s
 *
 * Of course, pure RISC processors don't have a signed multiply instruction.
 */
	.seg	"text"

        .globl  .mul
.mul:
        mov	%o0, %y
	andncc	%o0, 0xf, %o4
	be	.mul2
  	sethi	%hi(0xffff0000), %o5
        andncc	%o0, 0xff, %o4
        be,a	.mul4
 	mulscc	%o4, %o1, %o4
 	andncc	%o0, 0xfff, %o4
 	be,a	.mul6
 	mulscc	%o4, %o1, %o4
 	andcc	%o0, %o5, %o4
 	be,a	.mul8
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
 	orcc	%g0, %o0, %g0
 	rd	%y, %o0
 	bge	.mul0
 	orcc	%g0, %o0, %g0
 	sub	%o4, %o1, %o4
.mul0:
	bge	.mul1
 	mov	%o4, %o1
 	jmp	%o7 + 0x8
 	cmp	%o1, -0x1
.mul1:
	jmp	%o7 + 0x8
 	addcc	%o1, %g0, %g0
.mul2:
	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %o1, %o4
 	mulscc	%o4, %g0, %o4
 	rd	%y, %o5
	sll	%o4, 0x4, %o0
	srl	%o5, 0x1c, %o5
	orcc	%o5, %o0, %o0
	bge	.mul3
	sra	%o4, 0x1c, %o1
	jmp	%o7 + 0x8
	cmp	%o1, -0x1
.mul3:
	jmp	%o7 + 0x8
	addcc	%o1, %g0, %g0
.mul4:
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %o1, %o4
	mulscc	%o4, %g0, %o4
	rd	%y, %o5
	sll	%o4, 0x8, %o0
	srl	%o5, 0x18, %o5
	orcc	%o5, %o0, %o0
	bge	.mul5
	sra	%o4, 0x18, %o1
	jmp	%o7 + 0x8
	cmp	%o1, -0x1
.mul5:
	jmp	%o7 + 0x8
	addcc	%o1, %g0, %g0
.mul6:
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
	sll	%o4, 0xc, %o0
	srl	%o5, 0x14, %o5
	orcc	%o5, %o0, %o0
	bge	.mul7
	sra	%o4, 0x14, %o1
	jmp	%o7 + 0x8
	cmp	%o1, -0x1
.mul7:
	jmp	%o7 + 0x8
	addcc	%o1, %g0, %g0
.mul8:
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
	sll	%o4, 0x10, %o0
	srl	%o5, 0x10, %o5
	orcc	%o5, %o0, %o0
	bge	.mul9
	sra	%o4, 0x10, %o1
	jmp	%o7 + 0x8
	cmp	%o1, -0x1
.mul9:
	jmp	%o7 + 0x8
	addcc	%o1, %g0, %g0
