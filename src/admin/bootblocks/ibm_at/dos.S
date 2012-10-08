/*	@(#)dos.S	1.4	94/04/06 11:29:45 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * dos.S
 *
 * Using the standard disk layout it is possible to have multiple
 * operating systems residing on a single disk. This program boots
 * of DOS by mimicing the master bootstrap loader.
 * This program is stored on disk and made available in the kernel
 * directory, when executed this program will search the partition
 * table for a bootable DOS image and, if found, it will read the
 * first block of this partition at jumps to it.
 *
 * Author:
 *	Leendert van Doorn
 */
#include "boot.h"
#include "asdebug.h"

/* partition table constants */
#define	PART_NPART	4		/* # of partitions */
#define	PART_OFFSET	446		/* partition table offset */

/* boot signature */
#define	SIG_MAGIC	0xAA55		/* boot's magic number */
#define	SIG_OFFSET	0x01FE		/* signature offset */

/* partition system index */
#define	DOS_12		0x1		/* MS/DOS, 12 bit FAT */
#define	DOS_16		0x4		/* MS/DOS, 16 bit FAT */
#define	DOS_BIG		0x6		/* MS/DOS, BIG FAT, & UGLY :) */

#define LOADOFF		16 * LOADSEG
#define BIOSOFF		16 * BIOSSEG

.sect .text; .sect .rom; .sect .data; .sect .bss

	.sect	.text
/*
 * Setup stack and segment registers
 */
	jmpf	LOADSEG:start
start:
	cli
	mov	ax,cs
	mov	ds,ax
	mov	ss,ax
	mov	sp,8192
	mov	es,ax
	sti
	cld

/*
 * Say hello to let the world know we are here
 */
	push	greetings
	call	print
	add	sp,2

/*
 * Get the disk parameters (may be necessary to initialize it.)
 */
	push	es			! save es
	movb	ah,0x08
	movb	dl,0x80			! disk one
	int	0x13			! get parameters
	pop	es			! restore es
	jnb	mas_read
	push	paramerr
	jmp	error

/*
 * Read the master boot block
 */
mas_read:
	mov	di,3			! retry count
mas_retry:
	mov	bx,master_boot_block	! master boot block
	xorb	ch,ch			! start cylinder
	movb	cl,1			! start sector
	xorb	dh,dh			! start head
	movb	dl,0x80			! disk one
	mov	ax,0x201		! read one block
	int	0x13			! read sector
	jnb	check_master_boot
	dec	di
	je	finished		! retry if count > 0
	xorb	ah,ah			! but first
	int	0x13			! reset hard disk
	jmp	mas_retry
finished:
	push	mas_error		! print error message and die
	jmp	error

/*
 * Check validity of master boot block (stage one boot)
 */
check_master_boot:
	cmp	(master_boot_block + SIG_OFFSET), SIG_MAGIC
	je	walk_partition_table
	push	badmaster
	jmp	error

/*
 * Walk through the table looking for a DOS partition.
 */
walk_partition_table:
	mov	si,master_boot_block + PART_OFFSET
	movb	bl,PART_NPART
next_partition:
	cmpb	4(si),DOS_12		! is it a dos-12 partition ?
	je	foundit
	cmpb	4(si),DOS_16		! is it a dos-16 partition ?
	je	foundit
	cmpb	4(si),DOS_BIG		! is it a dos-BIG partition ?
	je	foundit
	add	si,16			! advance pointer
	decb	bl			! decrement count
	jne	next_partition		! try next
	push	missing			! sorry, not found
	jmp	error

foundit:
	mov	di,3			! retry count
dos_retry:
	mov	dx,(si)			! head
	movb	dl,0x80			! disk 0
	mov	cx,2(si)		! sector/cylinder
	mov	bp,si			! save index to partition table
	mov	ax,BIOSSEG		! es:bx, buffer to fill
	mov	es,ax
	xor	bx,bx
	mov	ax,0x201		! read one sector
	int	0x13			! read sector into memory
	jnb	check_signature
	xorb	ah,ah
	int	0x13			! reset hard disk
	dec	di
	jne	dos_retry		! retry if count > 0
	push	booterror
	jmp	error

/*
 * DOS's stage two bootstrap loader is loaded into core,
 * check signature and jump to it.
 */
check_signature:
	mov	ax,BIOSSEG
	mov	es,ax
	eseg	cmp (SIG_OFFSET),SIG_MAGIC
	je	bootstrap
	push	badmsdos
	jmp	error

/*
 * Call the DOS bootstrap.  This should be a simple matter of loading dl with
 * the disk code and es:si with the address of the partition table entry.
 * Alas it is not, to make some stupid bootstraps happy we need to have all
 * the segment registers zero.
 */
bootstrap:
	movb	dl,0x80			! select disk1 again
	mov	si,bp			! es:si = partition table entry
	add	si,LOADOFF		! adjust si to segment 0
	cli
	xor	ax,ax
	mov	ds,ax
	mov	es,ax
	mov	ss,ax			! ds = es = ss = 0
	mov	sp,BIOSOFF		! set sp where it is often set
	sti
	jmpf    0:BIOSOFF		! call bootstrap with cs = 0
	
/*
 * Print error message and die
 */
error:
	call	print
	add	sp,2
	call	dumpregs
error1:
	jmp	error1

/*
 * Print a message
 */
print:
	push	bp
	mov	bp,sp
	pusha
	mov	si,4(bp)
pr_next:
	movb	al,(si)
	inc	si
	cmpb	al, 0
	je	pr_done
	movb	ah,14
	movb	bl,1
	xorb	bh,bh
	int	0x10
	jmp	pr_next
pr_done:
	popa
	pop	bp
	ret

dumpregs:
	push	regheader
	call	print
	add	sp,2
	push	ax
	call	dumpreg
	pop	ax
	push	bx
	call	dumpreg
	pop	bx
	push	cx
	call	dumpreg
	pop	cx
	push	dx
	call	dumpreg
	pop	dx
	push	es
	call	dumpreg
	pop	es
	push	si
	call	dumpreg
	pop	si
	push	crlf
	call	print
	add	sp,2
	ret

dumpreg:
	push	bp
	mov	bp,sp
	pusha
	mov	bx,4(bp)
	mov	si,onereg+4
	mov	cx,4
hexdig:	dec	si
	movb	al,bl
	andb	al,0x0F
	cmpb	al,0x0A
	jae	highhex
	addb	al,0x30
	jmp	gotdig
highhex:addb	al,0x41-0x0A
gotdig:	movb	(si),al
	shr	bx,4
	loop	hexdig
	push	onereg
	call	print
	add	sp,2
	popa
	pop	bp
	ret

/*
 * Strings
 */
greetings:
	.asciz	"Loading MS/DOS ...\r\n"
paramerr:
	.asciz	"Error fetching disk parameters.\r\n"
mas_error:
	.asciz	"Error loading master bootstrap.\r\n"
booterror:
	.asciz	"Error loading operating system.\r\n"
missing:
	.asciz	"No DOS partition on this disk.\r\n"
badmaster:
	.asciz	"Bad master boot block.\r\n"
badmsdos:
	.asciz	"Bad MS/DOS boot block.\r\n"
regheader:
	.asciz	" ax    bx    cx    dx    es    si\r\n"
onereg:
	.asciz	"0000  "
crlf:
	.asciz	"\r\n"

/*
 * Place holder for the master boot block.
 */
master_boot_block:
	.space	512
