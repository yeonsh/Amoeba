/*	@(#)bootkernel.c	1.10	96/02/27 13:58:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * bootkernel.c
 *
 * Load a new kernel in memory and start it. This is very tricky since it
 * requires some fiddling with the 80386 protection mechanisms. Most of this
 * is done in khead.S, however.
 *
 * Author:
 *	Leendert van Doorn
 * Modified:
 *	Kees Verstoep, May 28 1993:
 *		Try to use the original buffer when it's big enough and safe.
 *	Kees Verstoep, November 1994:
 *		Modified to take segment alignments bytes (which are absent
 *		from the kernel binary) into account.
 */
#include <ack/h/out.h>
#include <amoeba.h>
#include <machdep.h>
#include <global.h>
#include <bootinfo.h>
#include <map.h>
#include <process/proc.h>
#include <i80386.h>
#include <type.h>
#include <seg.h>
#include <sys/proto.h>
#include <string.h>

/* fundamental constants */
#define LOWMEMSIZE	(640L * 1024L)	/* maximum low memory size */

#define NSECTS		4
#define SECT_TEXT	0
#define SECT_ROM	1
#define SECT_DATA	2
#define SECT_BSS	3

struct outsect kernelinfo[NSECTS];

extern void reboot();
void loadsegment();

/*
 * Boot a new kernel.
 * Cannot call panic when something went wrong,
 * since stop_kernel has already been executed.
 */
/* ARGSUSED */
void
bootkernel(kernelbase, kernelsize, commandline, flags)
    phys_bytes kernelbase;
    long kernelsize;
    char *commandline;
    int flags;
{
    register struct outhead *ohptr;
    phys_bytes kbase;
    struct bootinfo *bootinfo;
    phys_bytes loadaddr;
    vir_bytes loadsize;
    register int i;

    kbase = kernelbase;
    ohptr = (struct outhead *) kbase;
    if (ohptr->oh_magic != O_MAGIC) {
	printf("Kernel has bad magic\n");
	reboot();
    }
    /* The kernel must have NSECTS or NSECTS + 1 sections, depending on
     * whether it was stripped or not.
     */
    if (ohptr->oh_nsect < NSECTS || ohptr->oh_nsect > NSECTS + 1) {
	printf("Kernel has %d sections\n", ohptr->oh_nsect);
	reboot();
    }

    /*
     * Copy the section info from the binary to kernelinfo, since that
     * part may be overwritten in a moment.
     */
    kbase += sizeof(struct outhead);
    for (i = 0; i < NSECTS; i++) {
	kernelinfo[i] = * (struct outsect *) kbase;
	kbase += sizeof(struct outsect);
    }
    kernelinfo[SECT_BSS].os_size = (kernelinfo[SECT_BSS].os_size+0x0F) & ~0x0F;

    /* bound check: for the low kernels the size should be less than 640 Kb */
    if (kernelinfo[SECT_BSS].os_base < HI_MEM_START && 
        (kernelinfo[SECT_BSS].os_base + kernelinfo[SECT_BSS].os_size)
	 >= LOWMEMSIZE)
    {
	printf("Kernel (%d bytes) too large.\n", 
	       kernelinfo[SECT_BSS].os_base + kernelinfo[SECT_BSS].os_size);
	reboot();
    }

    /*
     * Load text, and data segments.  We try to set up the new kernel
     * in the original buffer if there is room, and if we're sure it won't
     * overwrite its own code while copying itself to the final destination.
     */
    loadsize = kernelinfo[SECT_BSS].os_base - kernelinfo[SECT_TEXT].os_base;
    if (loadsize <= kernelsize &&
        kernelbase > kernelinfo[SECT_TEXT].os_base + loadsize)
    {
	/* We can use the original buffer; there is no risk of damaging
	 * the data segment, because we're copying the segments (plus
	 * maybe a few alignment bytes) downwards.
	 */
	loadaddr = kernelbase;
	printf("bootkernel: use original buffer at 0x%lx\n", loadaddr);
	printf("loadsize = %ld, kernelsize = %ld\n", loadsize, kernelsize);
    }
    else
    {
	/* Need to copy it to a new buffer; try to allocate a suitable one */
	do {
	    loadaddr = (phys_bytes) getblk(loadsize);
	    if (loadaddr == 0) {
		printf("No buffer of size %d for new kernel\n", loadsize);
		reboot();
	    }
	} while (loadaddr < kernelinfo[SECT_TEXT].os_base + loadsize);
    }

    /* copy all segments at their proper place */
    printf("Building kernel at 0x%x\n", loadaddr);
    for (i = 0; i < NSECTS; i++) {
	loadsegment(kernelbase, loadaddr,
		    &kernelinfo[SECT_TEXT], &kernelinfo[i]);
    }

    printf("Kernel entry point at 0x%x\n", kernelinfo[SECT_TEXT].os_base);

#if defined(ISA) || defined(MCA)
    /*
     * Pass on command line arguments
     */
    bootinfo = (struct bootinfo *)BI_ADDR;
    bootinfo->bi_magic = BI_MAGIC;
    strncpy(bootinfo->bi_line, commandline, sizeof(bootinfo->bi_line));
#endif

    /*
     * Boot new kernel ...
     */
    (*((void (*)())loadaddr))(loadaddr,
			      (phys_bytes) kernelinfo[SECT_TEXT].os_base,
			      loadsize);
    printf("Kernel returned from boot\n");
    reboot();
    /* NOTREACHED */
}

/*
 * Copy segment to correct place in memory
 */
void
loadsegment(src, dest, firstsect, sect)
    phys_bytes src;
    phys_bytes dest;
    struct outsect *firstsect;
    struct outsect *sect;
{
    dest += sect->os_base - firstsect->os_base;
    if (sect->os_flen == 0) {
	printf("Loadsegment: zero segment of %d bytes at 0x%lx\n",
		sect->os_size, dest);
	/* zeroing of the bss is done by the kernel itself on startup */
    } else {
	printf("Loading %d bytes from 0x%x to 0x%x\n",
		sect->os_size, src + sect->os_foff, dest);
	(void) memmove((_VOIDSTAR) dest,
		       (_VOIDSTAR) (src + sect->os_foff),
		       (size_t) sect->os_flen);
	if (sect->os_flen < sect->os_size) {
	    printf("Zeroing %d alignment bytes\n",
		   sect->os_size - sect->os_flen);
	    (void) memset((_VOIDSTAR) (dest + sect->os_flen),
			  0, (size_t) (sect->os_size - sect->os_flen));
	}
    }
}
