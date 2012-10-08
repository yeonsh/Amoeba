/*	@(#)support.S	1.7	96/02/27 10:07:06 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * support.S
 *
 * This contains the machine language support routines which are
 * common to both the floppy disk and hard disk boot proper.
 *
 * Author:
 *	Leendert van Doorn
 */
#include "boot.h"
#include "asdebug.h"

.sect .text; .sect .rom; .sect .data; .sect .bss

	.sect	.text

/*
 * Start loaded kernel
 */
	.define	_startkernel
_startkernel:
	push	bp
	mov	bp, sp
	mov	ax,4(bp)		! data segment
	mov	ds, ax
	mov	es,ax
	mov	ss,ax
	jmpf	0:LOADADDR

/*
 * Halt the system
 */
	.define	_halt
	.define	EXIT
_halt:
EXIT:
	hlt
	jmp	_halt

/*
 * Return current value of DS register
 */
	.define	_dsseg
_dsseg:
	push	bp
	mov	bp,sp
	mov	ax,ds
	xor	dx,dx
	pop	bp
	ret

/*
 * Read block from disk using ROM BIOS
 */
	.extern _bios_diskread
_bios_diskread:
	push	bp
	mov	bp,sp
	push	es
	movb	dl,4(bp)	! boot disk
	movb	ch,6(bp)	! cylinder
	movb	dh,8(bp)	! head
	movb	cl,10(bp)	! sector
	les	bx,12(bp)	! es:bx pointer to buffer
	movb	al,16(bp)	! # of sectors to read
	movb	ah,0x02		! read disk
	int	0x13		! disk
	xchgb	al,ah		! status in al
	xorb	ah,ah		! zero ah
	pop	es
	pop	bp
	ret

/*
 * Reset disk system using ROM BIOS
 */
	.extern	_bios_diskreset
_bios_diskreset:
	push	bp
	mov	bp,sp
	movb	dl,4(bp)	! boot disk
	movb	ah,0		! reset disk system
	int	0x13		! disk
	pop	bp
	ret

/*
 * Retrieve disk parameters from BIOS ROM for the Amoeba winchester driver.
 */
	.extern	_bios_diskparams
_bios_diskparams:
	push	bp
	mov	bp,sp
	push	ds		! save ds
	push	ds
	pop	es
	mov	bx,4(bp)	! disk parameters vector
	mov	di,6(bp)	! parameter block
	xor	ax,ax		! ds points to base page
	mov	ds,ax
	shl	bx,1
	shl	bx,1
	mov	si,(bx)		! offset
	mov	ax,2(bx)	! segment
	mov	ds,ax
	mov	ax,0(si)	! # of cylinders
	eseg	mov  0(di),ax
	movb	al,2(si)	! # of heads
	eseg	movb 2(di),al
	mov	ax,5(si)	! starting write pre-comp cylinder
	shr	ax,1
	shr	ax,1
	eseg	mov  4(di),ax
	movb	al,8(si)	! control byte
	eseg	movb 6(di),al
	movb	al,14(si)	! # of sec/trk
	eseg	movb 7(di),al
	pop	ds		! restore ds
	pop	bp
	ret

/*
 * Don't wait for BIOS to turn of floppy disk motor,
 * just do it ourselfs.
 */
	.extern	_stop_motor
_stop_motor:
	mov	dx,0x3F2
	mov	ax,0x0C
	out	dx
	ret

/*
 * Get character from keyboard
 */
	.define _bios_getchar
_bios_getchar:
	push	bp
	movb	ah,1		! check input buffer
	int	0x16		! keyboard
	jnz	bios_getchar1	! character available
	mov	ax,-1		! no character
	pop	bp
	ret
bios_getchar1:
	movb	ah, 0		! read character from input
	int	0x16		! keyboard
	xorb	ah, ah
	movb	ah,14		! echo character
	movb	bl,1		! foreground color
	xorb	bh,bh		! page 0
	int	0x10		! bios video
	cmpb	al,0x0D		! if character == '\r'
	jne	bios_getchar2	! then print '\n'
	movb	al,0x0A		! new-line
	movb	ah,14		! echo character
	int	0x10		! bios video
bios_getchar2:
	pop	bp
	ret

/*
 * Put character on console
 */
	.define	_bios_putchar
_bios_putchar:
	push	bp
	mov	bp,sp
	movb	al,4(bp)	! character to print
	movb	ah,14		! print char
	movb	bl,1		! foreground color
	xorb	bh,bh		! page 0
	int	0x10		! bios video
	pop	bp
	ret

/*
 * Get real time clock values (they are in BCD)
 */
	.define	_bios_getrtc
_bios_getrtc:
	push	bp
	mov	bp,sp
	push	es
	push	si
	push	di
	movb	ah,0x02		! read real time clock
	int	0x1A
	mov	bx,4(bp)	! seconds
	xorb	ah,ah
	movb	al,dh
	mov	(bx),ax
	mov	bx,6(bp)	! minutes
	xorb	ah,ah
	movb	al,cl
	mov	(bx),ax
	mov	bx,8(bp)	! hours
	xorb	ah,ah
	movb	al,ch
	mov	(bx),ax
	pop	di
	pop	si
	pop	es
	pop	bp
	ret

/*
 * Copy a block of physical memory
 */
	.define	_phys_copy
_phys_copy:
	push	bp		! save only registers required by C
	mov	bp,sp		! set bp to point to source arg less 4
	push	si		! save si
	push	di		! save di
	push	ds		! save ds
	push	es		! save es
	mov	ax,4(bp)	! dx:ax = source address (dx is NOT segment)
	mov	dx,6(bp)
	mov	si,ax		! si = source offset = address % 16
	and	si,OFF_MASK
	andb	dl,HCHIGH_MASK	! ds = source segment = address / 16 % 0x10000
	andb	al,HCLOW_MASK
	orb	al,dl		! now bottom 4 bits of dx are in ax
	movb	cl,4		! rotate them to the top 4
	ror	ax,cl
	mov	ds,ax
	mov	ax,8(bp)	! dx:ax = destination addr (dx is NOT segment)
	mov	dx,10(bp)
	mov	di,ax		! di = dest offset = address % 16
	and	di,OFF_MASK
	andb	dl,HCHIGH_MASK	! es = dest segment = address / 16 % 0x10000
	andb	al,HCLOW_MASK
	orb	al,dl
	ror	ax,cl
	mov	es,ax
	mov	ax,12(bp)	! dx:ax = remaining count
	mov	dx,14(bp)
physcopy1:
	mov	cx,ax		! provisional count for this iteration
	test	ax,ax		! if count >= 0x8000, only do 0x8000 per iter
	js	physcopy2	! low byte already >= 0x8000
	test	dx,dx
	jz	physcopy3	! less than 0x8000
physcopy2:
	mov	cx,0x8000	! use maximum count per iteration
physcopy3:
	sub	ax,cx		! update count
	sbb	dx,0		! can't underflow, so carry clear now for rcr
	rcr	cx,1		! count in words, carry remembers if byte
!	jnc	physcopy4	! no odd byte
	jnb	physcopy4	! no odd byte
	movsb			! copy odd byte
physcopy4:
	rep	movs		! copy 1 word at a time
	mov	cx,ax		! test if remaining count is 0
	or	cx,dx
	jnz	physcopy5	! more to do
	pop	es		! restore es
	pop	ds		! restore ds
	pop	di		! restore di
	pop	si		! restore si
	pop	bp		! restore bp
	ret			! return to caller
physcopy5:
	sub	si,0x8000	! adjust pointers so the offset doesn't
	mov	cx,ds		! overflow in the next 0x8000 bytes
	add	cx,0x800	! pointers end up same physical location
	mov	ds,cx		! the current offsets are known >= 0x8000
	sub	di,0x8000	! since we just copied that many
	mov	cx,es
	add	cx,0x800
	mov	es,cx
	jmp	physcopy1	! start next iteration

/*
 * C epilogue routines (ought to fetch these from tail_cc)
 */
	.define .dsret, .sret, .cret
.dsret:
	pop     di
.sret:
	pop     si
.cret:
	mov     sp,bp
	pop     bp
	ret


 	.sect	.data

	.define	ECASE
ECASE = 20

	.define	hol0
hol0:
	.data2	0,0
	.data2	0,0

	.define	.trppc
.trppc:
	.data2   0

	.sect	.bss


