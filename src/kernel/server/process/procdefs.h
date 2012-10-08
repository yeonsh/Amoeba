/*	@(#)procdefs.h	1.5	94/04/06 10:02:40 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
** process management internal definitions.
*/
#define NPS_COLUMNS	3	/* # columns in ps directory */

#ifdef __STDC__

#if     !defined(_SIZE_T)
#define _SIZE_T
typedef unsigned int    size_t;         /* type returned by sizeof */
#endif  /* _SIZE_T */

#endif

/* From seg.c */

int		mem_free _ARGS(());
phys_bytes	SegAddr _ARGS((private *));
errstat		seg_create _ARGS((long, capability *, struct segtab **,
								long, long));
struct segtab *	seg_getprv _ARGS((rights_bits, private *));
struct segtab *	seg_grow _ARGS((struct segtab *, int, int));
void		seg_setprv _ARGS((struct segtab *, private *));
struct segtab *	seg_writeable _ARGS((struct segtab *));
errstat		usr_segcreate _ARGS((header *, capability *));
errstat		usr_segdelete _ARGS((private *));
char *		usr_segread _ARGS((header *, uint16 *));
errstat		usr_segwrite _ARGS((private *, long, char *, int, uint16*));
errstat		usr_segsize _ARGS((header *));

/* From map.c: */
phys_bytes	umap _ARGS((struct thread *, vir_bytes, vir_bytes, int));
segid		map_segment _ARGS((struct process *, vir_bytes, vir_bytes,
						long, capability *, long));
errstat		usr_mapseg _ARGS((struct process *, segment_d *));
errstat		usr_unmap _ARGS((struct process *, segid, capability *));
errstat		map_grow _ARGS((segid, long));
void		map_delmap _ARGS((struct process *));
void		initmap _ARGS(());
int		getmap _ARGS((struct process *, segment_d *, int, int));

/* from control.c */
struct mapping *GetThreadMap _ARGS((struct thread *));
errstat		ppro_getprv _ARGS((rights_bits, private *, struct process **));
errstat		create_process _ARGS((private *, process_d *, uint16));
void		ppro_list _ARGS((header *, char *, bufsize, char **, bufsize *));
void		ppro_lookup _ARGS((header *, char *, bufsize));
void		ppro_dirinit _ARGS((void));

/* from exception.c */
void		sendall _ARGS((struct process *, signum, int));
