/*	@(#)rem.s	1.2	94/04/11 14:18:49 */
/*
 * rem.s
 *
 * Of course, pure RISC processors don't have a remainder instruction.
 */
        .seg	"text"

	.global	.urem
.urem:
        ba	.rem1
	clr	%g1

	.global	.rem
.rem:
	orcc	%o1, %o0, %g0
	bge	.rem1
	mov	%o0, %g1
	orcc	%g0, %o1, %g0
	bge	.rem0
	orcc	%g0, %o0, %g0
	bge	.rem1
	sub	%g0, %o1, %o1
.rem0:
	sub	%g0, %o0, %o0
.rem1:
	orcc	%o1, %g0, %o5
	bne	.rem2
	mov	%o0, %o3
	ba	.rem30
	nop
.rem2:
	cmp	%o3, %o5
	blu	.rem28
	clr	%o2
	sethi	%hi(0x8000000),	%g2
	cmp	%o3, %g2
	blu	.rem10
	clr	%o4
.rem3:
	cmp	%o5, %g2
	bgeu	.rem5
	mov	0x1, %g3
	sll	%o5, 0x4, %o5
	ba	.rem3
	add	%o4, 0x1, %o4
.rem4:
	addcc	%o5, %o5, %o5
	bgeu	.rem5
	add	%g3, 0x1, %g3
	sll	%g2, 0x4, %g2
	srl	%o5, 0x1, %o5
	add	%o5, %g2, %o5
	ba	.rem6
	sub	%g3, 0x1, %g3
.rem5:
	cmp	%o5, %o3
	blu	.rem4
	nop
	be	.rem6
	nop
.rem6:
	subcc	%g3, 0x1, %g3
	bl	.rem27
	nop
	sub	%o3, %o5, %o3
	mov	0x1, %o2
	ba,a	.rem9
.rem7:
	sll	%o2, 0x1, %o2
	bl	.rem8
	srl	%o5, 0x1, %o5
	sub	%o3, %o5, %o3
	ba	.rem9
	add	%o2, 0x1, %o2
.rem8:
	add	%o3, %o5, %o3
	sub	%o2, 0x1, %o2
.rem9:
	subcc	%g3, 0x1, %g3
	bge	.rem7
	orcc	%g0, %o3, %g0
	ba,a	.rem27
.rem10:
	sll	%o5, 0x4, %o5
	cmp	%o5, %o3
	bleu	.rem10
	addcc	%o4, 0x1, %o4
	be	.rem28
	sub	%o4, 0x1, %o4
	orcc	%g0, %o3, %g0
.rem11:
	sll	%o2, 0x4, %o2
	bl	.rem19
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.rem15
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.rem13
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.rem12
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, 0xf, %o2
.rem12:
	addcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, 0xd, %o2
.rem13:
	addcc	%o3, %o5, %o3
	bl	.rem14
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, 0xb, %o2
.rem14:
	addcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, 0x9, %o2
.rem15:
	addcc	%o3, %o5, %o3
	bl	.rem17
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.rem16
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, 0x7, %o2
.rem16:
	addcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, 0x5, %o2
.rem17:
	addcc	%o3, %o5, %o3
	bl	.rem18
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, 0x3, %o2
.rem18:
	addcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, 0x1, %o2
.rem19:
	addcc	%o3, %o5, %o3
	bl	.rem23
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.rem21
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.rem20
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, -0x1, %o2
.rem20:
	addcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, -0x3, %o2
.rem21:
	addcc	%o3, %o5, %o3
	bl	.rem22
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, -0x5, %o2
.rem22:
	addcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, -0x7, %o2
.rem23:
	addcc	%o3, %o5, %o3
	bl	.rem25
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.rem24
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, -0x9, %o2
.rem24:
	addcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, -0xb, %o2
.rem25:
	addcc	%o3, %o5, %o3
	bl	.rem26
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, -0xd, %o2
.rem26:
	addcc	%o3, %o5, %o3
	ba	.rem27
	add	%o2, -0xf, %o2
.rem27:
	subcc	%o4, 0x1, %o4
	bge	.rem11
	orcc	%g0, %o3, %g0
	bl,a	.rem28
	add	%o3, %o1, %o3
.rem28:
	orcc	%g0, %g1, %g0
	bl,a	.rem29
	sub	%g0, %o3, %o3
.rem29:
	jmp	%o7 + 0x8
	mov	%o3, %o0
.rem30:
	ta	0x2
	jmp	%o7 + 0x8
	clr	%o0
