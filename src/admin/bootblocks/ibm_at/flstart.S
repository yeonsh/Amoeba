/*	@(#)flstart.S	1.5	94/04/06 11:30:00 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * flstart.S
 *
 * The floppy disk dependent part of the machine language assist code
 * for the Amoeba boot program.
 *
 * Author:
 *	Leendert van Doorn
 */
#include "boot.h"
#include "asdebug.h"

.sect .text; .sect .rom; .sect .data; .sect .bss

	.sect	.text
/*
 * Setup stack and segment registers
 */
	cli
	push	ds		! just to be safe
	pop	es
	mov     ax,BIOSSEG
        mov     ds,ax           ! set ds to cs
        mov     ss,ax           ! set ss to cs i.e., stack in high core
        mov     sp,STACKOFFSET	! initialize sp to high core
	sti

/*
 * Setup correct disk parameters.
 * First try a 1.44 Mb disk, if this fails try a 1.2 Mb disk.
 */
	mov	si,pspar	! PS/2 disk parameters
	call	setdiskparam	! set disk parameters
	xor	ax,ax
	mov	es,ax
	mov	ax,0x0201	! read sector, # sector = 1
	mov	bx,0x0600	! es:bx buffer
	mov	cx,0x0012	! track 0, sector 18
	movb	dh,0x00		! head 0
	int	0x13
	jnb	valid
	mov	si,atpar	! AT disk parameters
	call	setdiskparam	! set disk parameters
	xor	ax,ax
	mov	es,ax
	mov	ax,0x0201	! read sector, # sector = 1
	mov	bx,0x0600	! es:bx buffer
	mov	cx,0x000F	! track 0, sector 15
	movb	dh,0x00		! head 0
	int	0x13
	jnb	valid
	mov	bx,haltmsg	! print error message
	call	print
spin:
	jmp	spin		! spin for ever

/*
 * Read remainder of boot program into core
 */
valid:
	movb	ah,0x02		! disk read
	movb	al,17		! # of sectors to read
	movb	ch,0		! start cylinder
	movb	cl,1		! start sectors
	movb	dh,0		! start head
	mov	bx,BOOTSEG
	mov	es,bx
	xor	bx,bx		! es:bx -> buffer
	int	0x13		! BIOS disk

/*
 * Jump into high core
 */
	jmpf	BOOTSEG:start	! set cs to BOOTSEG
start:
	cli
	mov     ax,cs
	mov     ds,ax           ! set ds to cs
	mov     ss,ax           ! set ss to cs i.e., stack in high core
	mov     sp,STACKOFFSET	! initialize sp to high core
	xor     ax,ax
	mov     es,ax           ! set es to 0
	sti

	push	dx		! save disk code

/*
 * Reset video to 80x25
 */
	mov	ax,3
	int	0x10		! BIOS video

/*
 * Put cursor in ul corner
 */
        mov     ax,0x0200
        xor     bx,bx
        xor     dx,dx
        int     0x10		! BIOS video

	pop	dx		! restore disk code

/*
 * Set up boot disk geometry
 * Re-initialize the disk parameters since they might be
 * located in low (unsafe) memory.
 */
	movb	(_disk),dl	! disk.dk_diskid
	mov	(_disk+2),80	! disk.dk_cyl
	movb	(_disk+4),2	! disk.dk_heads
	movb	(_disk+5),18	! disk.dk_sectrk
	cmp	si, pspar
	je	is_ps
	movb	(_disk+5),15	! disk.dk_sectrk
is_ps:
	call	setdiskparam	! set disk parameters again using si.

/*
 * Setup C environment and jump to main()
 */
main:
	xor	ax, ax
	push	ax
	push	ax
	push	ax
	call	_main
	call	_halt
	/* NOTREACHED */

/*
 * Set disk parameters
 */
DSKBASE = 120			! ptr to disk parameters
setdiskparam:
	push	es
	xor	bx,bx
	mov	es,bx
	eseg	mov (DSKBASE),si
	eseg	mov (DSKBASE+2),ds
	xor	ax,ax		! disk system reset
	int	0x13
	pop	es
	ret

/*
 * Print a string
 */
print:	movb	al,(bx)
	testb	al,al
	jne	print1
	ret
print1:	movb	ah,14
	inc	bx
	push	bx
	movb	bl,1
	xorb	bh,bh
	int	0x10
	pop	bx
	jmp	print

haltmsg:.asciz	"Invalid floppy density.\r\nHalting system.\r\n"
pspar:	.data1	0xAF, 0x02, 25, 2, 18, 0x1B, 0xFF, 0x54, 0xF6, 1, 8 ! for ps
atpar:	.data1	0xDF, 0x02, 25, 2, 15, 0x1B, 0xFF, 0x54, 0xF6, 1, 8 ! for at


/*
 * Align up to next physical disk block
 * The installboot program fills locations 510 & 511
 * with the magic number.
 */
	.align	512

