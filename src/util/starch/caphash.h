/*	@(#)caphash.h	1.3	96/02/27 13:15:32 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Tunable */
/* Granularity of object allocation */
#define	HASH_GRANULE	1024

long	obj_exists();
long	findobj();
int	cap_eq();
void	id_hash();
void	init_hash();

struct obj_table {
    int		size;		/* Number of entries malloc'd in objs table*/
    int		num_entries;	/* Number of entries in use in objs table */
    long	*objs;
} ;

extern	struct obj_table *hash_tab; /* The main hash_table */

