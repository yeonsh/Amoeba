/*	@(#)coff2bsd.h	1.3	94/04/07 14:03:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __COFF2BSD_H__
#define __COFF2BSD_H__

/* Definitions for BSD/COFF compatibility.
   Include this file after <a.out.h>, if a.out.h is COFF-style
   (a convenient #define to test for is '#ifdef ISCOFF').
   This will allow programs expecting BSD a.out headers to work
   with COFF files.
   Authors: Kyuho Kim, Jack Jansen.
*/
struct exec {
	FILHDR	filehdr;
	AOUTHDR aouthdr;
	SCNHDR  txthdr, dathdr, bsshdr;
}; 

#define	a_text	aouthdr.tsize
#define	a_data	aouthdr.dsize
#define	a_bss	aouthdr.bsize
#define	a_entry	aouthdr.entry
#define	a_syms	filehdr.f_nsyms
#define	a_magic	filehdr.f_magic

#define N_DATOFF(x)	((x).dathdr.s_scnptr)
#define N_SYMOFF(x)	((x).filehdr.f_symptr)
#define N_STROFF(x)	(N_SYMOFF(x) + (x).filehdr.f_nsyms)

#define N_TXTADDR(x)	((x).aouthdr.text_start)
#define N_DATADDR(x)	((x).aouthdr.data_start)
#define N_BSSADDR(x)	((x).aouthdr.bss_start)

#ifdef i386
# define N_BADMAG(x)	((x).a_magic != I386MAGIC)
#endif

#ifdef mips
/*
** XXX.
** It's probably better to include "aouthdr.h", but that results in 
** an enormous amount of compile errors.
*/
# ifdef N_BADMAG
#  undef N_BADMAG
# endif
# define N_BADMAG(x)	((x).a_magic != MIPSELMAGIC && \
			 (x).a_magic != MIPSEBMAGIC)
#endif

#ifndef N_BADMAG
# include "Roll your own N_BADMAG macro!"
#endif

#endif /* __COFF2BSD_H__ */
