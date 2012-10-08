/*	@(#)hdstart.S	1.6	96/02/27 10:04:34 */
/*
 * Copyright 1996 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * hdstart.S
 *
 * The hard disk dependent part of the machine language assist code
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
	mov     ax,BIOSSEG
        mov     ds,ax           ! set ds to cs
        mov     ss,ax           ! set ss to cs i.e., stack in high core
        mov     sp,STACKOFFSET	! initialize sp to high core (1 Kb stack)
	sti

/*
 * Save the offset to the boot partition.
 */
	eseg	push 10(si)	! high 16 bits
	eseg	push 8(si)	! low 16 bits

/*
 * Read Amoeba boot into high core
 * The master boot program passes on a pointer to (this)
 * active partition (es:si) and drive number (dl).
 */
	movb	ah,0x02		! disk read
	movb	al,17		! # of sectors to read
	eseg	movb ch,3(si)	! start cylinder
	eseg	movb cl,2(si)	! start sectors
	eseg	movb dh,1(si)	! start head
	mov	bx,BOOTSEG
	mov	es,bx
	xor	bx,bx		! es:bx -> buffer
	int	0x13		! BIOS disk

/*
 * Boot partition offset (cx:bx)
 */
	pop	bx
	pop	cx

/*
 * Jump into high core
 */
	jmpf	BOOTSEG:start	! set cs to BOOTSEG
start:
	cli
	mov     ax,cs
	mov     ds,ax           ! set ds to cs
	mov     ss,ax           ! set ss to cs i.e., stack in high core
	mov     sp,STACKOFFSET	! initialize sp to high core (1 Kb stack)
	xor     ax,ax
	mov     es,ax           ! set es to 0
	sti

/*
 * Set part_off to the boot partition offset, it will be used to find
 * the partition we booted from in the partition table.
 */
	mov	(_part_off+0),bx
	mov	(_part_off+2),cx

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
 */
	movb	(_disk),dl	! disk.dk_diskid
	push	es		! save es
	movb	ah,0x08
	int	0x13
	pop	es		! restore es
	movb	al,cl
	andb	al,0x3F
	movb	(_disk+5),al	! disk.dk_sectrk
	xor	ax,ax
	movb	al,cl
	shl	ax,1
	shl	ax,1
	movb	al,ch
	inc	ax
	mov	(_disk+2),ax	! disk.dk_cyl
	incb	dh
	movb	(_disk+4),dh	! disk.dk_heads

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
 * Align up to next physical disk block
 * The installboot program fills locations 510 & 511
 * with the magic number.
 */
	.align	512
