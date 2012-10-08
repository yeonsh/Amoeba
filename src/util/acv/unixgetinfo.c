/*	@(#)unixgetinfo.c	1.5	96/02/27 12:42:33 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef NO_UNIX_AOUT

#include <stdlib.h>
#include "acv.h"

/* In case an ANSI compiler is used in combination with the standard system
 * headers, we may need to define a few more system-specific symbols before
 * including a.out.h.
 */
#if defined(__sun__) && !defined(sun)
#define sun
#endif
#if defined(__sparc__) && !defined(sparc)
#define sparc
#endif
#if defined(__mc68000__) && !defined(mc68000)
#define mc68000
#endif

#ifdef AMOEBA
#include "byteorder.h"
#include "my_aout.h"

/*
 * We only support gcc/gld-created (NMAGIC/bigendian) a.out binaries.
 */
#define NMAGIC  0410            /* binary created with gld -n */

#define N_BADMAG(x) (((x).a_magic & 0xFFFF) != NMAGIC)
#define N_TXTOFF(x) MY_TXTOFF(x)
#define N_DATOFF(x) MY_DATOFF(x)
#define N_SYMOFF(x) MY_SYMOFF(x)
#define N_STROFF(x) MY_STROFF(x)

#else /* !AMOEBA */
/* include a.out.h without the system's nlist.h */
#define __NLIST_H__
#define _nlist_h
#define _SYMS_H
#include "a.out.h"
#endif /* AMOEBA */

#ifdef ISCOFF	/* COFF support (sort of..) */
#include "amoeba.h"
#include "machdep.h"
#include "coff2bsd.h"
#include "sys/seg.h"
#if mips
#define MY_TXTOFF(x) N_TXTOFF(x.filehdr, x.aouthdr)
#endif
#include <sys/types.h>
#include <sys/stat.h>
#endif


/* The N_* macros are defined on Suns and in coff2bsd.h, but not in BSD.
 * OFF macros define file offsets, ADDR macros define VM addresses.
 */

/* N_TXTOFF is standard BSD: sizeof struct exec or 1024 depending on a_magic;
 * but for v7, we define it:
 */

#ifndef N_TXTOFF
#define N_TXTOFF(x)	(sizeof (struct exec))		/* For v7 Unix */
#endif
#ifndef MY_TXTOFF
#define MY_TXTOFF(x)	N_TXTOFF(x)
#endif

#ifndef N_TXTADDR
#define N_TXTADDR(x)    0x400
/* This value works only if you always specify -T 400 to the loader.
 * For other architectures where this doesn't work (and which don't define
 * N_TXTADDR in <a.out.h>), you may have to fix this.
 */
#endif

#ifndef N_DATOFF
#define N_DATOFF(x)	(MY_TXTOFF(x) + (x).a_text)
#endif

#ifndef N_DATADDR
#define N_DATADDR(x)	(N_TXTADDR(x) + (x).a_text)
#endif

#ifndef N_BSSADDR
#define N_BSSADDR(x)	(N_DATADDR(x) + (x).a_data)
#endif

/* Getting the target addresses right for the mc68000 and the sparc
 * requires some magic.  The problem is that things like, e.g., the start of
 * the text segment as used by the loader is not present in the a.out header.
 * What we do is the following: from the entry point we determine whether
 * we are looking at a sparc or mc68000 binary.  We use that, and a
 * few assumptions regarding the way the segments are aligned by default,
 * to compute the segment start addresses.
 */

#define ALIGN(addr,seg)	(seg * (((addr) + seg - 1) / seg))

/* The following defines reflect the way in which we currently link our
 * binaries with both the Sun and the GNU loader:
 */
#define SPARC_PAGESIZE	0x10000
#define SPARC_SEGSIZE	0x10000
#define SPARC_TXTADDR	0x40000
#define SPARC_ENTRY	SPARC_TXTADDR

#define M68000_PAGESIZE	0x2000
#define M68000_SEGSIZE	0x20000
#define M68000_TXTADDR	M68000_PAGESIZE
#define M68000_ENTRY	M68000_PAGESIZE
#define M68000_ENTRY2	(M68000_PAGESIZE+0x20)

#ifdef sparc
/* In this case the Amoeba one is certainly different from the Unix one */
#undef  N_TXTADDR
#define N_TXTADDR(x)    SPARC_TXTADDR
#endif

long lseek();

static void
convert_exec(execp)
struct exec *execp;
{
#ifdef AMOEBA
    /* Only need to convert on Amoeba.  On Unix, the exec header should
     * already be of the correct endianness.
     */
    dec_l_be(&execp->a_magic);
    dec_l_be(&execp->a_text);
    dec_l_be(&execp->a_data);
    dec_l_be(&execp->a_bss);
    dec_l_be(&execp->a_syms);
    dec_l_be(&execp->a_entry);
    dec_l_be(&execp->a_trsize);
    dec_l_be(&execp->a_drsize);
#endif
}

int
isunixaout(fd)
int fd;
{
    struct exec exec_buf;

    if (lseek(fd, 0L, 0) != 0L) {
	return 0;
    }
    if (read(fd, (char *)&exec_buf, sizeof(exec_buf)) != sizeof(exec_buf)) {
	return 0;
    }
    convert_exec(&exec_buf);

    return !N_BADMAG(exec_buf);
}

char *
unixgetinfo(fd, aip)
int fd;
struct aoutinfo *aip;
{
    struct exec aout_head;

    /* read and check a.out header */
    if (lseek(fd, 0L, 0) != 0L) {
	return "lseek failed";
    }
    if (read(fd, (char *)&aout_head, sizeof(aout_head)) != sizeof(aout_head)) {
	return "failed to read a.out header";
    }
    convert_exec(&aout_head);
    if (N_BADMAG(aout_head)) {
	return "not a unix type a.out file";
    }

    /* By default, assume architecture is the same as this host */
    aip->ai_architecture = 0;

#if defined(sparc) || defined(mc68000) || defined(AMOEBA)
    /* some trickery to be able to convert mc68000 on a sparc and vice versa */
    if (aout_head.a_entry == SPARC_ENTRY) {
	aip->ai_architecture = "sparc";
    } else if (aout_head.a_entry == M68000_ENTRY ||
	       aout_head.a_entry == M68000_ENTRY2) {
	aip->ai_architecture = "mc68000";
    } else {
	printf("Warning: unexpected entry point 0x%lx\n", aout_head.a_entry);
	printf("Assuming sparc architecture\n");
	aip->ai_architecture = "sparc";
    }
#endif

    if (aip->ai_architecture != 0) {
	if (strcmp(aip->ai_architecture, "mc68000") == 0) {
	    aip->ai_txt_virtual = M68000_TXTADDR;
	    aip->ai_dat_virtual =
		ALIGN(aip->ai_txt_virtual + aout_head.a_text, M68000_SEGSIZE);
	    aip->ai_bss_virtual = 
		ALIGN(aip->ai_dat_virtual + aout_head.a_data, M68000_PAGESIZE);
	} else if (strcmp(aip->ai_architecture, "sparc") == 0) {
	    aip->ai_txt_virtual = SPARC_TXTADDR;
	    aip->ai_dat_virtual =
		ALIGN(aip->ai_txt_virtual + aout_head.a_text, SPARC_SEGSIZE);
	    aip->ai_bss_virtual =
		ALIGN(aip->ai_dat_virtual + aout_head.a_data, SPARC_PAGESIZE);
	} else {
	    /* not reached, really */
	    return "unsupported architecture";
	}
    } else {
	/* Assume they're the same as on this Unix */
	aip->ai_txt_virtual = N_TXTADDR(aout_head);
	aip->ai_dat_virtual = N_DATADDR(aout_head);
	aip->ai_bss_virtual = N_BSSADDR(aout_head);
    }

    aip->ai_txt_offset = MY_TXTOFF(aout_head);
    aip->ai_txt_size = aout_head.a_text;

    aip->ai_dat_offset = N_DATOFF(aout_head);
    aip->ai_dat_size = aout_head.a_data;

    aip->ai_bss_size = aout_head.a_bss;

    /*
     * read the symbol table
     */

#ifdef ISCOFF
    /* We don't convert COFF currently, because I don't have access
     * to a machine producing it.  We just put the entire symbol+string
     * table in the Amoeba string table (which, unlike the symboltable,
     * is immune for byteswaps).
     */
    aip->ai_sym_size = 0;
    aip->ai_sym_tab = 0;
#else
    aip->ai_sym_size = aout_head.a_syms;

    if (aip->ai_sym_size == 0) {
	aip->ai_sym_tab = 0;
    } else {
	if ((aip->ai_sym_tab = malloc(aip->ai_sym_size)) == 0) {
	    return "couldn't allocate symbol table";
	}
	if (lseek(fd, (long)N_SYMOFF(aout_head), 0) != N_SYMOFF(aout_head)) {
	    return "seek to symbol table failed";
	}
	if (read(fd, aip->ai_sym_tab, aip->ai_sym_size) != aip->ai_sym_size) {
	    return "read of symbol table failed";
	}

#ifdef AMOEBA
	{
	    /* Convert the symbols to our endian */
	    int i, nsyms;

	    nsyms = aip->ai_sym_size / sizeof(struct nlist);
	    for (i = 0; i < nsyms; i++) {
		struct nlist *sym;

		sym = ((struct nlist *) aip->ai_sym_tab) + i; 
		dec_l_be((long *) &sym->n_name);
		dec_s_be(&sym->n_desc);
		dec_l_be(&sym->n_value);
	    }
	}
#endif /* AMOEBA */
    }
#endif

    /*
     * read the string table
     */

#ifdef ISCOFF
    {
	struct stat buf;

	/* For COFF, we put the entire symbol+string+whatever table in the
	 * Amoeba string table. It reaches from N_SYMOFF to end of file.
	 */
	if (fstat(fd, &buf) != 0) {
		return "couldn't get file status";
	}
	aip->ai_str_size = buf.st_size - N_SYMOFF(aout_head) + sizeof(long);
        if (lseek(fd, (long)N_SYMOFF(aout_head), 0) != N_SYMOFF(aout_head)) {
	    return "seek to symbol table failed";
        }
    }
#else
    if (aip->ai_sym_size == 0) {
	aip->ai_str_size = 0; /* empty symbol table => empty string table */
    } else {
	/* first read how big it is */
	if (lseek(fd, (long)N_STROFF(aout_head), 0) != N_STROFF(aout_head)) {
	    return "seek to string table failed";
	}
	if (read(fd, (char *)&aip->ai_str_size, sizeof(long))!= sizeof(long)) {
	    return "couldn't read string table length";
	}
#ifdef AMOEBA
	dec_l_be(&aip->ai_str_size);
#endif
    }
#endif

    if (aip->ai_str_size == 0) {
	aip->ai_str_tab = 0;	/* no string table */
    } else {
	if ((aip->ai_str_tab = malloc(aip->ai_str_size)) == 0) {
	    return "couldn't allocate string table";
	}
	/* first 4 bytes a full string table is the byte count */
	*(long *)aip->ai_str_tab = aip->ai_str_size;

	/* read the string table itself */
	if (read(fd, aip->ai_str_tab + sizeof(long),
		 aip->ai_str_size - sizeof(long))
	      != aip->ai_str_size - sizeof(long)) {
	    return "couldn't read string table";
	}
    }

    /*
     * and finally set the entry point
     */
    aip->ai_entrypoint = aout_head.a_entry;

    return 0;
}

#else /* NO_UNIX_AOUT */
extern int main(); /* avoid "empty translation unit" */
#endif
