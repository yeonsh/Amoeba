/*	@(#)nm.c	1.5	96/02/27 13:09:57 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* nm - print name list.		Author: Dick van Veen */

/* Dick van Veen: veench@cs.vu.nl */
/* Ported to Amoeba by versto@cs.vu.nl */

#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef AMOEBA
#include "amoeba.h"
#include "stderr.h"
#include "namelist.h"
#include "module/name.h"
#include "module/proc.h"
#else
#include <a.out.h>
#endif

#ifndef _VOIDSTAR
#define _VOIDSTAR void *
#endif

/* Read the name list in memory, sort it, and print it.  */

/* Nm [-dgnopru] [file] ...
 *
 * flags:
 *	-a	all symbols, including the ones containing debug info
 *	-d	address in decimal
 *	-g	print only external symbols.
 *	-n	sort numerically rather than alphabetically.
 *	-o	prepend file name to each line rather than only once.
 *	-p	don't sort, pint n symbol-table order.
 *	-r	sort in reverse order.
 *	-u	print only undefined symbols.
 *
 *	-	when no file name is present, a.out is assumed.
 *
 *	NOTE:	no archives are supported because assembly files don't
 *		have symbol tables.
 *
 */

#define A_OUT		"a.out"

int a_flag;
int d_flag;
int g_flag;
int n_flag;
int o_flag;
int p_flag;
int r_flag;
int u_flag;

char io_buf[BUFSIZ];		/* io buffer */
#ifndef AMOEBA
struct exec aout_head;		/* header of a.out file */
#endif
long stbl_elems;		/* #elements in symbol table */

/*ARGSUSED*/
main(argc, argv)
int argc;
char **argv;
{
  argv++;
  while (*argv != 0 && **argv == '-') {
	*argv += 1;
	while (**argv != '\0') {
		switch (**argv) {
		    case 'a':	a_flag = 1;	break;
		    case 'd':	d_flag = 1;	break;
		    case 'g':	g_flag = 1;	break;
		    case 'n':	n_flag = 1;	break;
		    case 'o':	o_flag = 1;	break;
		    case 'p':	p_flag = 1;	break;
		    case 'r':	r_flag = 1;	break;
		    case 'u':	u_flag = 1;	break;
		    default:
			fprintf(stderr, "illegal flag: -%c\n", **argv);
			exit(-1);
		}
		*argv += 1;
	}
	argv++;
  }
  setbuf(stdin, io_buf);
  if (*argv == 0)
	nm(A_OUT);
  else
	while (*argv != 0) {
		nm(*argv);
		argv++;
	}
  exit(0);
}

#ifndef __STDC__
#define const /**/
#endif

#ifdef _MINIX

#define sym_name(sym)	((sym)->n_name)
#define sym_is_ext(sym)	((sym)->n_sclass & N_CLASS) == C_EXT)
#define name_cmp(s1,s2)	(strncmp((s1), (s2), 8))

#else

long strtab_len;	/* more specifically: 4 bytes */
char *string_table;

#define sym_is_ext(sym)	(((sym)->n_type & N_EXT) == N_EXT)
#define name_cmp(s1,s2)	(strcmp((s1), (s2)))

#define n_sclass	n_type
#define N_SECT		N_TYPE

char *
sym_name(sym)
register struct nlist *sym;
{
    register long off = sym->n_un.n_strx;

    if (0 < off && off < strtab_len) {	/* should be offset in string list */
	return(string_table + off);
    } else {
	return "";
    }
}

#endif

nm_sort(tmp_stbl1, tmp_stbl2)
const _VOIDSTAR tmp_stbl1;
const _VOIDSTAR tmp_stbl2;
{

  struct nlist *stbl1 = (struct nlist *)tmp_stbl1;
  struct nlist *stbl2 = (struct nlist *)tmp_stbl2;
  int cmp;

  if (n_flag) {			/* sort numerically */
	if ((stbl1->n_sclass & N_SECT) <
	    (stbl2->n_sclass & N_SECT))
		cmp = -1;
	else if ((stbl1->n_sclass & N_SECT) >
		 (stbl2->n_sclass & N_SECT))
		cmp = 1;
	else if (stbl1->n_value < stbl2->n_value)
		cmp = -1;
	else if (stbl1->n_value > stbl2->n_value)
		cmp = 1;
	else
		cmp = name_cmp(sym_name(stbl1), sym_name(stbl2));
  } else {
	cmp = name_cmp(sym_name(stbl1), sym_name(stbl2));
	if (cmp == 0) {
		if (stbl1->n_value < stbl2->n_value)
			cmp = -1;
		else if (stbl1->n_value > stbl2->n_value)
			cmp = 1;
	}
  }

  if (r_flag) cmp = -cmp;	/* reverse sort */
  return(cmp);
}


nm(file)
char *file;
{
  struct nlist *stbl;

#ifndef AMOEBA
  int fd;
  off_t position;

  if ((fd = read_header(file)) < 0) {
      return;
  }
  if (aout_head.a_syms == 0) {
	close(fd);
	return;
  }
  if ((size_t) aout_head.a_syms != aout_head.a_syms) {
	fprintf(stderr, "%s: symbol table too large to allocate\n", file);
	close(fd);
	return;
  }
  if ((int) aout_head.a_syms != aout_head.a_syms) {
	/* This is necessary because we are too lazy to iterate the read. */
	fprintf(stderr, "%s: symbol table too large to read\n", file);
	close(fd);
	return;
  }

  stbl = (struct nlist *) malloc((size_t) aout_head.a_syms);
  if (stbl == NULL) {
	fprintf(stderr, "%s: can't allocate symbol table\n", file);
	close(fd);
	return;
  }
  if (read(fd, (char *)stbl, (unsigned)aout_head.a_syms) != aout_head.a_syms) {
	fprintf(stderr, "%s: can't read symbol table\n", file);
	free((_VOIDSTAR)stbl);
	close(fd);
	return;
  }
  stbl_elems = (int) aout_head.a_syms / sizeof(struct nlist);

#ifndef _MINIX
  /*
   * Allocate space for the string table and read it into memory.
   */

  position = lseek(fd, 0L, SEEK_CUR); /* get current position */

  if (read(fd, (char *)&strtab_len, sizeof(strtab_len)) != sizeof(strtab_len)){
      fprintf(stderr, "%s: can't read string table\n", file);
      free((_VOIDSTAR)stbl);
      close(fd);
      return;
  }

  /* The integer strtab_len itself is also part of the string table.
   * The real name strings directly follow that integer.
   */

  if ((string_table = malloc(strtab_len)) == NULL) {
      fprintf(stderr, "%s: can't allocate string table\n", file);
      free((_VOIDSTAR)stbl);
      close(fd);
      return;
  }
  
  lseek(fd, position, SEEK_SET); /* seek back, and read string table */
  if (read(fd, string_table, strtab_len) != strtab_len) {
      fprintf(stderr, "%s: can't read string table\n", file);
      free((_VOIDSTAR)stbl);
      free((_VOIDSTAR)string_table);
      close(fd);
      return;
  }

#endif /* !_MINIX */

#else /* AMOEBA */
  if (read_symtab(file, &stbl, &stbl_elems, &string_table, &strtab_len) != 0) {
      return;
  }
#endif

  if (!p_flag) qsort(stbl, stbl_elems, sizeof(struct nlist), nm_sort);
  nm_print(file, stbl);

  free((_VOIDSTAR)stbl);
#ifndef _MINIX
  free((_VOIDSTAR)string_table);
#endif

#ifndef AMOEBA
  close(fd);
#endif
}

#ifdef AMOEBA
static int get_sym_off_size(/* file, sym_off, sym_size */);

read_symtab(file, stbl, stbl_elems, strtab, strtab_len)
char *file;
struct nlist **stbl;
long *stbl_elems;
char **strtab;
long *strtab_len;
{
    off_t sym_off;
    long  sym_size;
    int   fd = -1;
    char *whole_symtab = NULL;

#   define Failure()	{ goto failure; }

    fd = open(file, O_RDONLY);
    if (fd < 0) {
	fprintf(stderr, "can't open %s\n", file);
	Failure();
    }

    if (get_sym_off_size(file, &sym_off, &sym_size) != 0) {
	/* problem already reported */
	Failure();
    }

    if ((whole_symtab = (char *) malloc((size_t) sym_size)) == NULL) {
	fprintf(stderr, "cannot allocate symbol table\n");
	Failure();
    }	

    /* read in the symbol table */
    lseek(fd, sym_off, SEEK_SET);
    if (read(fd, whole_symtab, (int)sym_size) != sym_size) {
	fprintf(stderr, "cannot read symbol table\n");
	Failure();
    }

    /* convert it to our endianness so that we can make sense out of it */
    if (buf_get_symtab(whole_symtab, whole_symtab + sym_size,
		       stbl, stbl_elems, strtab, strtab_len) != 0) {
	fprintf(stderr, "cannot convert symbol table\n");
	Failure();
    }
    *stbl_elems /= sizeof(struct nlist);

    /* success! */
    close(fd);
    free((_VOIDSTAR)whole_symtab);
    return(0);

failure:
    /* undo the things we did before we encountered the problem */
    if (fd >= 0) close(fd);
    if (whole_symtab != NULL) free((_VOIDSTAR)whole_symtab);
    return(1);
}

#else /* not AMOEBA */

read_header(file)
char *file;
{
  off_t aout_off, sym_off;
  int bytes, fd;

  fd = open(file, O_RDONLY);
  if (fd == -1) {
      fprintf(stderr, "can't open %s\n", file);
      return(-1);
  }
  bytes = read(fd, (char *)&aout_head, sizeof(struct exec));
  if (bytes != sizeof(struct exec) || BADMAG(aout_head)) {
      fprintf(stderr, "%s: no executable file\n", file);
      close(fd);
      return(-1);
  }

  lseek(fd, A_SYMPOS(aout_head), SEEK_SET);
  return(fd);
}

#endif /* AMOEBA */

nm_print(file, stbl)
char *file;
register struct nlist *stbl;
{
  struct nlist *last;
  int n_sclass;
  char type;
#ifndef _MINIX
  char *name;
#else
  char name[9];

  name[8] = '\0';
#endif

  if (!o_flag) printf("%s:\n", file);
  for (last = &stbl[stbl_elems]; stbl != last; stbl++) {
	if (g_flag && !sym_is_ext(stbl)) continue;
	if (u_flag && (stbl->n_sclass & N_SECT) != N_UNDF) continue;

	n_sclass = stbl->n_sclass & N_SECT;
	if (n_sclass == N_ABS)
		type = 'a';
	else if (n_sclass == N_TEXT)
		type = 't';
	else if (n_sclass == N_DATA)
		type = 'd';
	else if (n_sclass == N_BSS)
		type = 'b';
	else {
		if (!a_flag && !u_flag) {
			/* By default skip other symbols, like the ones
			 * containing debug info.
			 */
			continue;
		}
		type = 'u';
	}
	if (sym_is_ext(stbl)) type += 'A' - 'a';
#ifdef _MINIX
	strncpy(name, sym_name(stbl), 8);
#else
	name = sym_name(stbl);
#endif
	if (strlen(name) == 0 && !a_flag) {
		/* by default skip symbol entries with no proper name */
		continue;
	}
	if (d_flag) {
		/* Offsets in decimal. */
		if (o_flag) 
		       printf("%s:%08ld %c %s\n",file,stbl->n_value,type,name);
		else
		       printf("%08ld %c %s\n", stbl->n_value, type, name);
	} else {
		/* Offsets in hex. */
		if (o_flag) 
		       printf("%s:%08lx %c %s\n",file,stbl->n_value,type,name);
		else
		       printf("%08lx %c %s\n", stbl->n_value, type, name);
	}
  }
}

#ifdef AMOEBA

static int
get_sym_off_size(file, sym_off, sym_size)
char	*file;
off_t	*sym_off;
long	*sym_size;
/* Try to get the symbol table offset & size.
 * Returns 1 on failure, 0 on success.
 */
{
    capability	filecap;
    errstat	status;
    process_d  *proc_desc;
    int		seg, notfound;

    if ((status = name_lookup(file, &filecap)) != STD_OK) {
        fprintf(stderr, "cannot lookup %s (%s)\n", file, err_why(status));
	return(1);
    }

    if ((proc_desc = pd_read(&filecap)) == NULL) {
	fprintf(stderr, "%s: no process descriptor\n", file);
	return(1);
    }

    /* find the symbol table segment */
    notfound = 1;	/* pessimistic as ever */
    for (seg = 0; seg < proc_desc->pd_nseg; seg++) {
	if ((&PD_SD(proc_desc)[seg])->sd_type & MAP_SYMTAB) {
	    /* got it */
	    *sym_off = (PD_SD(proc_desc)[seg]).sd_offset;
	    *sym_size = (PD_SD(proc_desc)[seg]).sd_len;
	    notfound = 0;
	    break;
	}
    }

    if (notfound) {
	fprintf(stderr, "%s has no symbol table\n", file);
    } else {
	/* we only handle new style symbol tables, having an extra flag */
	if (((&PD_SD(proc_desc)[seg])->sd_type & MAP_NEW_SYMTAB) == 0) {
	    fprintf(stderr, "%s does not have a new style symbol table\n",
		    file);
	    notfound = 1;
	}
    }

    free((_VOIDSTAR)proc_desc); /* not needed anymore */

    return(notfound);
}

#endif /* AMOEBA */
