/*	@(#)divide.s	1.2	94/04/11 14:14:20 */
/*
 * div.s
 *
 * Of course, pure RISC processors don't have a divide instruction.
 */
        .seg	"text"

        .global	.udiv
.udiv:
	ba	.div1
	clr	%g1

	.global	.div
.div:
	orcc	%o1, %o0, %g0
	bge	.div1
	xor	%o1, %o0, %g1
	orcc	%g0, %o1, %g0
	bge	.div0
	orcc	%g0, %o0, %g0
	bge	.div1
	sub	%g0, %o1, %o1
.div0:
	sub	%g0, %o0, %o0
.div1:
	orcc	%o1, %g0, %o5
	bne	.div2
	mov	%o0, %o3
	ba	.div30
	nop
.div2:
	cmp	%o3, %o5
	blu	.div28
	clr	%o2
	sethi	%hi(0x8000000),	%g2
	cmp	%o3, %g2
	blu	.div10
	clr	%o4
.div3:
	cmp	%o5, %g2
	bgeu	.div5
	mov	0x1, %g3
	sll	%o5, 0x4, %o5
	ba	.div3
	add	%o4, 0x1, %o4
.div4:
	addcc	%o5, %o5, %o5
	bgeu	.div5
	add	%g3, 0x1, %g3
	sll	%g2, 0x4, %g2
	srl	%o5, 0x1, %o5
	add	%o5, %g2, %o5
	ba	.div6
	sub	%g3, 0x1, %g3
.div5:
	cmp	%o5, %o3
	blu	.div4
	nop
	be	.div6
	nop
.div6:
	subcc	%g3, 0x1, %g3
	bl	.div27
	nop
	sub	%o3, %o5, %o3
	mov	0x1, %o2
	ba,a	.div9
.div7:
	sll	%o2, 0x1, %o2
	bl	.div8
	srl	%o5, 0x1, %o5
	sub	%o3, %o5, %o3
	ba	.div9
	add	%o2, 0x1, %o2
.div8:
	add	%o3, %o5, %o3
	sub	%o2, 0x1, %o2
.div9:
	subcc	%g3, 0x1, %g3
	bge	.div7
	orcc	%g0, %o3, %g0
	ba,a	.div27
.div10:
	sll	%o5, 0x4, %o5
	cmp	%o5, %o3
	bleu	.div10
	addcc	%o4, 0x1, %o4
	be	.div28
	sub	%o4, 0x1, %o4
	orcc	%g0, %o3, %g0
.div11:
	sll	%o2, 0x4, %o2
	bl	.div19
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.div15
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.div13
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.div12
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, 0xf, %o2
.div12:
	addcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, 0xd, %o2
.div13:
	addcc	%o3, %o5, %o3
	bl	.div14
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, 0xb, %o2
.div14:
	addcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, 0x9, %o2
.div15:
	addcc	%o3, %o5, %o3
	bl	.div17
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.div16
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, 0x7, %o2
.div16:
	addcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, 0x5, %o2
.div17:
	addcc	%o3, %o5, %o3
	bl	.div18
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, 0x3, %o2
.div18:
	addcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, 0x1, %o2
.div19:
	addcc	%o3, %o5, %o3
	bl	.div23
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.div21
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.div20
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, -0x1, %o2
.div20:
	addcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, -0x3, %o2
.div21:
	addcc	%o3, %o5, %o3
	bl	.div22
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, -0x5, %o2
.div22:
	addcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, -0x7, %o2
.div23:
	addcc	%o3, %o5, %o3
	bl	.div25
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	bl	.div24
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, -0x9, %o2
.div24:
	addcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, -0xb, %o2
.div25:
	addcc	%o3, %o5, %o3
	bl	.div26
	srl	%o5, 0x1, %o5
	subcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, -0xd, %o2
.div26:
	addcc	%o3, %o5, %o3
	ba	.div27
	add	%o2, -0xf, %o2
.div27:
	subcc	%o4, 0x1, %o4
	bge	.div11
	orcc	%g0, %o3, %g0
	bl,a	.div28
	sub	%o2, 0x1, %o2
.div28:
	orcc	%g0, %g1, %g0
	bl,a	.div29
	sub	%g0, %o2, %o2
.div29:
	jmp	%o7 + 0x8
	mov	%o2, %o0
.div30:
	ta	0x2
	jmp	%o7 + 0x8
	clr	%o0
