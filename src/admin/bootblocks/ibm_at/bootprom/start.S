/*	@(#)start.S	1.2	96/02/27 10:09:51 */
/*
 * start.S
 *
 * Assembler start off
 *
 * Copyright (c) 1992 by Leendert van Doorn
 */
#include "param.h"

#define	OFF_MASK    0x000F	/* offset mask for physb to hclick:offset */
#define	HCHIGH_MASK 0x0F	/* h/w click mask for low byte of hi word */
#define	HCLOW_MASK  0xF0	/* h/w click mask for low byte of low word */

.sect .text; .sect .rom; .sect .data; .sect .bss

#ifdef NOROM
/*
 * Non ROM startoff. This part assumes that it is loaded using the
 * Amoeba bootstrap loader (i.e. at LOADSEG). From there on it copies
 * itself out of the way and simulates the BIOS ROM search code.
 */
#define	BOOTADDR	0x7C400
#define	BOOTSEG		0x07C40
	.sect	.text
	jmpf	LOADSEG:START
START:
	cli
	mov	ax,BOOTSEG
	mov	ss,ax
	mov	sp,32*1024
	xor	si,si
	mov	ax,LOADSEG
	mov	ds,ax
	xor	di,di
	mov	ax,BOOTSEG
	mov	es,ax
	mov	cx,16 * 1024
	cld
	rep	movsb
	sti
	callf	BOOTSEG:ROM
ROM:
#endif /* NOROM */

/*
 * During POST, interrupt vectors are established for the BIOS calls.
 * After the default vectors are in place, a scan for additional ROM
 * modules is made. The addresses 0xC8000 through 0xE0000 are scanned
 * in 2Kb block (the addresses 0xE0000 through 0xEFFFF are also scanned
 * but these are usually not available for "user" purposes). When an
 * ROM is identified (i.e. it has the valid magic 0x55AA) a far call
 * is made to it.
 */
	.sect	.text
	.data1	0x55, 0xAA		! magic ROM identifier
	.data1	0x00			! size of ROM
	jmp	romstart		! 2 bytes jump
	.data1	0x00			! check sum
romstart:
	cli
	cld

/*
 * Zero RAM image where the ROM copy will reside
 */
	mov	ax,cs
	mov	ds,ax
	mov	ax,RAMSEG
	mov	es,ax
	mov	cx,RAMSIZE
	xor	di,di
	xorb	al,al
	rep	stosb

/*
 * Copy ROM image to RAM
 */
	xor	si,si
	xor	di,di
	mov	cx,ROMSIZE
	rep	movsb

/*
 * Setup stack, and segment registers, and jump
 * to the shadow copy in RAM.
 */
	mov	ax,RAMSEG
	mov	ds,ax
	mov	es,ax
	mov	ss,ax
	mov	sp,stack_top
	jmpf	RAMSEG:ramstart
ramstart:
	sti

/*
 * Reset video to 80x25
 */
	mov	ax,2
	int	0x10		! BIOS video

/*
 * Put cursor in ul corner
 */
	mov	ax,0x0200
	xor	bx,bx
	xor	dx,dx
	int	0x10		! BIOS video

/*
 * Setup C environment and jump to main()
 */
	xor	ax, ax
	push	ax
	push	ax
	push	ax
	call	_main
	call	_halt
	/* NOTREACHED */

/*
 * Start previously loaded program
 */
	.define	_execute
_execute:
	jmpf	0:LOADADDR

/*
 * Get real time clock values (they are in BCD)
 */
	.define	_getrtc
_getrtc:
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
 * Put character on console
 */
	.define	_putchar
_putchar:
	push	bp
	mov	bp,sp
	push	es
	movb	al,4(bp)	! character to print
	movb	ah,14		! print char
	movb	bl,1		! foreground color
	xorb	bh,bh		! page 0
	int	0x10		! bios video
	pop	es
	pop	bp
	ret

#ifdef MONITOR
/*
 * Get character from console and echo it
 */
	.define _getchar
_getchar:
	push	bp
	mov	bp,sp
	push	es
	movb	ah, 0		! read character from input
	int	0x16		! keyboard
	movb	ah,14		! echo character
	movb	bl,1		! foreground color
	xorb	bh,bh		! page 0
	int	0x10		! bios video
	cmpb	al,0x0D		! if character == '\r'
	jne	getchar1	! then print '\n'
	movb	al,0x0A		! new-line
	movb	ah,14		! echo character
	int	0x10		! bios video
getchar1:
	pop	es
	pop	bp
	ret

/*
 * Install keyboard break handler. The environment is saved much
 * in the fasion of the old V6 Unix setexit/reset mechanism.
 */
	.define	_brkhandler
_brkhandler:
	cli
	push	es
	xor	ax,ax		! save old interrupt handler
	mov	es,ax
	eseg	mov ax,(4*BRK_INTR)
	mov	(oldintr),ax
	eseg	mov ax,(4*BRK_INTR+2)
	mov	(oldintr+2),ax
	mov	ax,ds		! install new interrupt handler
	eseg	mov (4*BRK_INTR),brkintr
	eseg	mov (4*BRK_INTR+2),ax
	pop	es
	sti
	mov	bx,environ	! save current environment
	pop	cx		! return address
	mov	0(bx),si
	mov	2(bx),di
	mov	4(bx),bp
	mov	6(bx),sp
	mov	8(bx),cx	! return address
	mov	10(bx),RAMSEG	! return segment
	xor	ax,ax
	jmp	cx

/*
 * The user has typed CTRL BREAK. Restore the environment saved
 * during a call to brkhandler and return 1. An EOI is generated
 * since according to the documentation this interrupt may be
 * generated from another interrupt routine.
 */
brkintr:
	cli
	mov	ax,0x20		! pic's EOI
	outb	0x20
	outb	0xA0
	mov	ax,RAMSEG
	mov	ds,ax
	mov	es,ax
	mov	ss,ax
	mov	bx,environ
	mov	si,0(bx)
	mov	di,2(bx)
	mov	bp,4(bx)
	mov	sp,6(bx)
	mov	ax,1
	sti
	jmpf	8(bx)

/*
 * Reset the keyboard break interrupt vector to its old
 * settings.
 */
	.define	_brkreset
_brkreset:
	cli
	push	es
	xor	ax,ax
	mov	es,ax
	mov	ax,(oldintr)
	eseg	mov (4*BRK_INTR),ax
	mov	ax,(oldintr+2)
	eseg	mov (4*BRK_INTR+2),ax
	pop	es
	sti
	ret
#endif /* MONITOR */

/*
 * Hard reboot using the keyboard controller
 */
	.define	_reboot
_reboot:
	mov	ax,0xFC
	outb	0x64
	/* FALL THROUGH */

/*
 * Bios disk boot
 */
	.define	_diskboot
_diskboot:
	int	0x19
	/* FALL THROUGH */

/*
 * Halt processor. Don't halt the processor or disable interrupts
 * since that prevents the user from manually rebooting the machine.
 */
	.define	EXIT
	.define	_halt
EXIT:
_halt:
	sti
	jmp	_halt

/*
 * Copy block of memory
 */
	.define	_bcopy
_bcopy:
	push	bp
	mov	bp,sp
	push	si
	push	di
	mov	si,4(bp)	! source
	mov	di,6(bp)	! destination
	mov	cx,8(bp)	! count
	cld
	rep	movsb
	pop	di
	pop	si
	pop	bp
	ret

#ifdef notyet
/*
 * Compare one block of memory against another
 */
	.define	_bcmp
_bcmp:
	push	bp
	mov	bp,sp
	push	si
	push	di
	mov	si,4(bp)	! source
	mov	di,6(bp)	! destination
	mov	cx,8(bp)	! count
	xor	ax,ax
	cld
	rep	cmpsb
	jcxz	bcmp1
	cmpsb
	jz	bcmp1
	inc	ax
bcmp1:
	pop	di
	pop	si
	pop	bp
	ret
#endif

/*
 * Zero a block of memory
 */
	.define	_bzero
_bzero:
	push	bp
	mov	bp,sp
	push	di
	mov	di,4(bp)	! source
	mov	cx,6(bp)	! count
	xorb	al,al
	cld
	rep	stosb
	pop	di
	pop	bp
	ret

/*
 * Read an (unsigned) byte from an I/O port
 */
	.define	_in_byte
_in_byte:
	push	bp
	mov	bp,sp
	mov	dx,4(bp)	! port
	inb	dx
	xorb	ah,ah
	pop	bp
	ret

/*
 * Write a byte to an I/O port
 */
	.define	_out_byte
_out_byte:
	push	bp
	mov	bp,sp
	mov	dx,4(bp)	! port
	mov	ax,6(bp)	! value
	outb	dx
	pop	bp
	ret

/*
 * Read an (unsigned) 16-bit word from an I/O port
 */
	.define	_in_word
_in_word:
	push	bp
	mov	bp,sp
	mov	dx,4(bp)	! port
	in	dx
	pop	bp
	ret

/*
 * Write a 16-bit word to an I/O port
 */
	.define	_out_word
_out_word:
	push	bp
	mov	bp,sp
	mov	dx,4(bp)	! port
	mov	ax,6(bp)	! value
	out	dx
	pop	bp
	ret

/*
 * Return current value of DS register
 */
	.define	_dsseg
_dsseg:
	mov	ax,ds
	xor	dx,dx
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
/*
 * ACK necesities
 */
	.define	ECASE
ECASE = 20

	.define	hol0
hol0:
	.data2	0,0
	.data2	0,0

	.define	.trppc
.trppc:
	.data2	0
	.data1	0


	.sect	.bss
/*
 * Old interrupt handler and monitor environment
 */
oldintr:
	.space	4
environ:
	.space	6*2

/*
 * Program stack
 */
stack_bottom:
	.space	1024
stack_top:

