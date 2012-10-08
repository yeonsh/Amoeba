/*	@(#)aging.h	1.1	96/02/27 14:06:48 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* This file contains the definitions used for aging files. */

/* Methods for accessing age info */
/* These are the value of BASE and LIMIT, intended to be stored
   into an unsigned 8-bit byte. `legal' values are 0..254,
   255 represents `unknown'.
 */

#define L_LIMIT		127
#define L_UNKNOWN	255

#ifdef MAX_LIFETIME
# if MAX_LIFETIME>L_LIMIT
abort MAX_LIFETIME>L_LIMIT
# endif
#else
abort unknown MAX_LIFETIME
#endif

/* Threshold values for warning others */
/* These must be the same among all storage servers!! */
#define	SOFT_THRESHOLD	2
#define HARD_THRESHOLD	3

/* The current base value for the lifetime counters */

extern int	bs_ttl_base ;
	

/* General access method */
#define L_READ_CVT(val)	(val==L_UNKNOWN?L_UNKNOWN:\
	((int)val-bs_ttl_base+3*L_LIMIT+1)%(2*L_LIMIT+1)-L_LIMIT)
#define L_VAL(ttl)	(bs_ttl_base+(ttl))%(2*L_LIMIT+1)

/* AGE_IN_INODE determines whether the age information is stored
   in the inode, or in  a seperate table.
   undefine if the last option is preferred.
 */
#define AGE_IN_INODE	1

#ifdef AGE_IN_INODE
/* Access method for lifetime values */
# define XS_LOCAL_LIFE(i)	Inode_table[(i)].i_l_age
# define XS_GLOBAL_LIFE(i)	Inode_table[(i)].i_g_age
#else
extern	unsigned char	*Inode_local_ttl ;
extern	unsigned char	*Inode_global_ttl ;
# define XS_LOCAL_LIFE(i)	(Inode_local_ttl[(i)])
# define XS_GLOBAL_LIFE(i)	(Inode_global_ttl[(i)])
#endif

/* Read acces to `real' values of lifetimes */
#define LOCAL_TTL(i)	L_READ_CVT((unsigned char)(XS_LOCAL_LIFE(i)))
#define GLOBAL_TTL(i)	L_READ_CVT((unsigned char)(XS_GLOBAL_LIFE(i)))

/* Write access to lifetime values */
#define	INIT_LOCAL_TTL(i)	(XS_LOCAL_LIFE(i) = L_UNKNOWN)
#define	INIT_GLOBAL_TTL(i)	(XS_GLOBAL_LIFE(i) = L_UNKNOWN)
#define SET_LOCAL_TTL(i,ttl)	(XS_LOCAL_LIFE(i) = L_VAL(ttl))
#define SET_GLOBAL_TTL(i,ttl)	(XS_GLOBAL_LIFE(i) = L_VAL(ttl))
