/*	@(#)ackgetinfo.c	1.2	93/01/15 14:53:55 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include "acv.h"
#include "namelist.h"
#include "out.h"

/* segment numbers: */
#define TEXTSG	0
#define ROMSG	1
#define DATASG	2
#define BSSSG	3
#define LSECT	BSSSG+1
#define NSECT	LSECT+1

/*
 * Header and section table of new ACK format object file.
 */
static struct outhead	outhead;
static struct outsect	outsect[NSECT];

static void read_symtab();
long lseek();

int
isackaout(fd)
int fd;
{
    struct outhead out_buf;
    short ack_magic;
    char *magic_ptr;

    if (lseek(fd, 0L, 0) != 0L) {
	return 0;
    }
    if (read(fd, (char *)&out_buf, sizeof(out_buf)) != sizeof(out_buf)) {
	return 0;
    }

    /* check the ack header oh_magic field in an byteorder indepent way: */
    magic_ptr = (char *)&out_buf.oh_magic;
    ack_magic = ((magic_ptr[1] & 0xff) << 8) | (magic_ptr[0] & 0xff);
    return (ack_magic  == O_MAGIC);
}

char *
ackgetinfo(fd, aip)
int fd;
struct aoutinfo *aip;
{
    /* read and check a.out header */
    if (lseek(fd, 0L, 0) != 0L) {
	return "lseek failed";
    }
    if (!rd_fdopen(fd)) {
	return "bad rd_fdopen";
    }

    rd_ohead(&outhead);
    if (BADMAGIC(outhead)) {
	return "not an ack a.out file";
    }
    if (outhead.oh_flags & HF_LINK) {
	return "unresolved references in ack a.out file";
    }
    if (outhead.oh_nsect != LSECT && outhead.oh_nsect != NSECT) {
	return "wrong number of sections in ack a.out file";
    }
    rd_sect(outsect, outhead.oh_nsect);

    aip->ai_architecture = 0;		/* No idea */
    
    aip->ai_txt_offset = outsect[TEXTSG].os_foff;
    aip->ai_txt_virtual = outsect[TEXTSG].os_base;
    aip->ai_txt_size = outsect[TEXTSG].os_size;
    
    aip->ai_rom_offset = outsect[ROMSG].os_foff;
    aip->ai_rom_virtual = outsect[ROMSG].os_base;
    aip->ai_rom_size = outsect[ROMSG].os_size;
    
    aip->ai_dat_offset = outsect[DATASG].os_foff;
    aip->ai_dat_virtual = outsect[DATASG].os_base;
    aip->ai_dat_size = outsect[DATASG].os_size;
    
    aip->ai_bss_virtual = outsect[BSSSG].os_base;
    aip->ai_bss_size = outsect[BSSSG].os_size;
    
    if (outhead.oh_nsect == NSECT) { /* symbol table available */
	read_symtab(aip);
    } else {
	aip->ai_sym_tab = aip->ai_str_tab = NULL;
	aip->ai_sym_size = aip->ai_str_size = 0;
    }
    
    aip->ai_entrypoint = outsect[TEXTSG].os_base;
    
#ifdef DEBUG
    {
	register int i;
    
	for (i = 0; i < outhead.oh_nsect; i++) {
	    fprintf(stderr, "Sect %d: base 0x%x, size 0x%x, ",
		    i, outsect[i].os_base, outsect[i].os_size);
	    fprintf(stderr, "foff 0x%x, flen 0x%x, align 0x%x\n",
		    outsect[i].os_foff, outsect[i].os_flen,outsect[i].os_lign);
	}
    }
#endif

    return(NULL);	/* indicating success */
}

rd_fatal()
{
	fprintf(stderr, "Fatal error during read of ack a.out file\n");
	exit(-1);
}

/* Symbol table conversion code is derived from ~ack/mach/sun3/cv/cv.c: */
/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 *
 */

static int rom_in_data = 0;  /* rom always put in the read-only text segment */

extern void fatal();

static int
is_rest_local(A, i)
register struct outname *A;
register unsigned int i;
{
    while (i--) {
	if (A->on_type & S_EXT) return 0;
	A++;
    }
    return 1;
}

static void
read_symtab(aip)
struct aoutinfo *aip;
{
    struct outname *ACKnames;		/* ack symbol table */
    struct nlist *MACHnames = NULL;	/* unix symbol table */
    char *Chars = NULL;			/* string table */

    register unsigned int i = outhead.oh_nname;
    register struct outname *A;
    register struct nlist *M;
    long offX = OFF_CHAR(outhead) - 4;

    if ((A = (struct outname *)calloc(i, sizeof(struct outname))) == NULL) {
	fatal("couldn't allocate ack symbol table");
    }
    rd_name(A, outhead.oh_nname);

    if ((M = (struct nlist *)calloc(i, sizeof(struct nlist))) == NULL) {
	fatal("couldn't allocate symbol table");
    }

    MACHnames = M;
    ACKnames = A;
    for (; i; i--, A++) {
	M->n_value = A->on_valu;
	M->n_desc = A->on_desc;
	if ((A->on_type & S_SCT) ||
	    (A->on_type & S_ETC) == S_FIL) {
	    static int rest_local = 0;

	    if (rest_local || (rest_local = is_rest_local(A, i))) {
		outhead.oh_nname--;
		continue;
	    }
	}
	if (A->on_type & S_STB) {
	    M->n_type = A->on_type >> 8;
	} else if (A->on_type & S_COM) {
	    M->n_type = N_UNDF | N_EXT;
	} else {
	    switch(A->on_type & S_TYP) {
	    case S_UND:
		switch(A->on_type & S_ETC) {
		case S_MOD:	 M->n_type = N_FN;	break;
		case S_LIN:	 M->n_type = N_ABS;	break;
		default:	 M->n_type = N_UNDF;	break;
		}
		break;
	    case S_ABS:		 M->n_type = N_ABS;	break;
	    case S_MIN + TEXTSG: M->n_type = N_TEXT; 	break;
	    case S_MIN + ROMSG:	 M->n_type = (rom_in_data ? N_DATA : N_TEXT);
				 break;
	    case S_MIN + DATASG: M->n_type = N_DATA;	break;
	    case S_MIN + BSSSG:	 M->n_type = N_BSS;	break;
	    case S_MIN + LSECT:	 M->n_type = N_BSS;	break;
	    default:
		fprintf(stderr,"warning: unknown s_type: %d\n",
			A->on_type & S_TYP);
	    }
	}
	if (A->on_type & S_EXT) M->n_type |= N_EXT;
	M->n_un.n_strx = A->on_foff;
	M++;
    }

    free((char *)ACKnames);

    M = MACHnames;
    for (i = outhead.oh_nname; i; i--, M++) {
	if (M->n_un.n_strx) {
	    M->n_un.n_strx -= offX;
	} else {
	    M->n_un.n_strx = outhead.oh_nchar + 3; /* pointer to nullbyte */
	}
    }

    if ((unsigned) outhead.oh_nchar != outhead.oh_nchar ||
	!( Chars = (char *) malloc((unsigned) (sizeof(long) + outhead.oh_nchar)))) {
	fatal("couldn't allocate string table");
    }
    /* set long prefix and read string table after it */
    *(long *)Chars = sizeof(long) + outhead.oh_nchar;
    rd_string(Chars + sizeof(long), outhead.oh_nchar);

    /* put symbol table and string table in the *aip structure */
    aip->ai_sym_tab = (char *)MACHnames;
    aip->ai_sym_size = outhead.oh_nname * sizeof(struct nlist);
    aip->ai_str_tab = Chars;
    aip->ai_str_size = sizeof(long) + outhead.oh_nchar;
}
