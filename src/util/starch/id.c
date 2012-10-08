/*	@(#)id.c	1.6	96/02/27 13:15:46 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/* STARCH - STore and reSTore ARCHives of graphs.
   module implementing the internal capability table
*/

#include "starch.h"

/* The `unreachable' name */

char			*noname= "?\??\??\??" ;

/* A simple implementation */

static	ptr		idtable[10] ;

static char		*namestore ;
static int		namelength ;
static int		nameptr ;

void			id_unlock();

void
init_id() {
}

static struct cap_t *
lid_dget(pp_id,offset)
	register struct cap_t	**pp_id;
	long			offset;
{
	int			index ;

	if ( !pp_id ) fatal("internal: lid/lev1, zero ptr ptr") ;
	if ( !*pp_id ) {
		*pp_id=(struct cap_t *)getmem(N_ID_IN_BLOCK*sizeof (struct cap_t)) ;
		/* Set suite pointers */
		for ( index=0 ; index<N_ID_IN_BLOCK ; index++ ) {
			cn_capdst(*pp_id+index).cs_suite=
				cn_suite(*pp_id+index);
			cn_capdst(*pp_id+index).cs_final= CAPS_IN_ID ;
		}
	}
	
	if ( offset>=N_ID_IN_BLOCK ) {
		fatal("internal: error 1 in id addressing");
	}
	return *pp_id+offset ;
}

static struct cap_t *
lid_iget(pp_id,offset,level)
	register struct cap_t	***pp_id;
	unsigned long		offset;
	int			level;
{
	unsigned long		new_offset ;
	unsigned long		id_per_level ;
	int			i ;
	int			index ;

	if ( !pp_id ) fatal("internal: lid/indir, zero ptr ptr") ;
	if ( !*pp_id ) {
		*pp_id=(struct cap_t **)getmem(N_ID_IN_BLOCK*sizeof (struct cap_t **)) ;
	}
	for ( id_per_level=1, i=2 ; i<=level ; i++ ) {
		id_per_level *= N_ID_IN_BLOCK ;
	}
	index= offset/id_per_level ; new_offset= offset-index*id_per_level ;
	if ( index>=N_ID_IN_BLOCK ) {
		fatal("internal: error 2 in id addressing");
	}
	if ( level==2 ) {
		return lid_dget(*pp_id+index,new_offset) ;
	} else {
		return lid_iget(*pp_id+index,new_offset,level-1) ;
	}
}

struct cap_t *
lid_get(id)
	unsigned long		id;
{
	int			index ;
	unsigned long		id_offset ;

	index= id/N_ID_IN_BLOCK ;
	if ( index<7 ) {
		id_offset= index*N_ID_IN_BLOCK ;
		return lid_dget(&idtable[index],id-id_offset) ;
	} else {
		id_offset= 7*N_ID_IN_BLOCK ;
		index-= 7 ;
		if ( index<N_ID_IN_BLOCK ) {
			/* Double indirect */
			return lid_iget(&idtable[7],id-id_offset,2) ;
		} else {
			id_offset += N_ID_IN_BLOCK*N_ID_IN_BLOCK ;
			index-= N_ID_IN_BLOCK ;
			index/= N_ID_IN_BLOCK ;
			if ( index<N_ID_IN_BLOCK ) {
				/* Triple indirect */
				return lid_iget(&idtable[8],id-id_offset,3);
			} else {
				/* Quadruple indirect */
				id_offset += N_ID_IN_BLOCK*
						N_ID_IN_BLOCK*N_ID_IN_BLOCK ;
				return lid_iget(&idtable[9],id-id_offset,4);
			}
		}
	}
}

struct cap_t *
id_get(id)
	long			id;
{
	register struct cap_t	*p_id ;
	long			tid ;

	p_id=lid_get(id);

	if ( cn_flag(p_id)&LINKED ) {
		tid=cn_parent(p_id) ;
		id_unlock(p_id) ;
		return id_get(tid) ;
	}
	return p_id ;
}

long
id_link(id)
	long			id;
{
	/* Returns the `linked to' number of the indicated id */
	register struct cap_t	*p_id ;
	long			tid ;

	p_id=lid_get(id);

	if ( cn_flag(p_id)&LINKED ) {
		tid=cn_parent(p_id) ;
		id_unlock(p_id) ;
		return id_link(tid) ;
	}
	id_unlock(p_id);
	return id ;
}

void
id_unlock(p_id)
	struct cap_t	*p_id ;
{
	/* currently empty */
	return ;
#if 0
	if ( cn_lock(p_id)>0 ) cn_lock(p_id)-- ;
#endif
}

static void addname(name)
	char		*name;
{
	int			name_length ;

	name_length=strlen(name) ;
	if ( nameptr+name_length>namelength-1 ) {
		namelength+=256 ;
		namestore=(char *)adjmem((ptr)namestore,(unsigned)namelength) ;
	}
	if ( nameptr ) namestore[nameptr++]= '/' ;
	strcpy(&namestore[nameptr],name) ;
	nameptr+= name_length ;
}

void id_apn(p_id,id)
	register struct cap_t	*p_id ;
	long			id ;
{
	register struct cap_t	*p_pid ;
	int			entry_count ;
	register struct dir_ent	*next_entry ;
	int			i ;
	char			*name ;

	if ( cn_parent(p_id)==0 ) {
		if ( id!=1 ) {
			addname(noname) ;
		}
		return ;
	}
	p_pid= id_get(cn_parent(p_id)) ;
	if ( cn_type(p_pid)!=C_DIR ) {
		warning("internal, inconsistent graph, id %ld",id);
		addname(noname) ;
		id_unlock(p_pid) ;
		return ;
	}
	if ( !cn_dir(p_pid) ) {
		warning("internal, missing directory information, id %ld",id);
		addname(noname) ;
		id_unlock(p_pid) ;
		return ;
	}
	entry_count= cn_dir(p_pid)->r_count ;
	next_entry= cn_dir(p_pid)->r_entries ;
	if ( !next_entry ) {
		warning("internal, missing directory information, id %ld",id);
		addname(noname) ;
		id_unlock(p_pid) ;
		return ;
	}


	id_apn(p_pid,cn_parent(p_id)) ;

	name= 0 ;
	for ( i=0 ; i<entry_count ; i++, next_entry++ ) {
		if ( acq_cap(next_entry) == id ) {
			/* Ok found the name */
			name= next_entry->name ;
			break ;
		}
	}
	if ( name ) {
		addname(name) ;
	} else {
		/* None found, put some ??? */
		addname(noname) ;
	}
	id_unlock(p_pid) ;
}
	
			
char *
id_pn(id)
	long		id ;
{
	register struct cap_t	*p_id ;
	
	/* get the name of a file */
	/* this is complicated because we know the id if a parent, but
	   that is all. Thus we have to scan the parent for the name
	   and put that into the buffer. Starting with the `oldest' parent.
	   This called for a recursive algorithm.
	*/
	if ( namelength==0 ) {
		namelength=256 ;
		namestore=(char *)getmem((unsigned)namelength) ;
	}

	nameptr=0 ; namestore[0]=0 ;
	p_id=id_get(id) ;
	id_apn(p_id,id) ;
	id_unlock(p_id) ;

	return namestore ;
	
}

#ifdef MEMDEBUG
id_cleanup() {
	/* Cleanup memory used by id table */
	register struct cap_t	*p_id ;
	register struct cap_t	**pp_id ;
	register struct cap_t	**pp_id2 ;
	register struct cap_t	***pp_id3 ;
	int			i,j,k ;

	/* Single indirect */
	for ( i=0 ; i<7 ; i++ ) {
		id_cln((struct cap_t *)idtable[i]) ;
		idtable[i]= 0 ;
	}

	/* Double indirect */
	pp_id= (struct cap_t **) idtable[7];
	if ( pp_id ) {
		for ( i=0 ; i<N_ID_IN_BLOCK ; i++ ) {
			id_cln((struct cap_t *)pp_id[i]) ;
		}
		free((char *)pp_id) ;
		idtable[7]= 0;
	}

	/* Triple indirect */
	pp_id2= (struct cap_t **) idtable[8];
	if ( pp_id2 ) {
		for ( i=0 ; i<N_ID_IN_BLOCK ; i++ ) {
			pp_id= (struct cap_t **) pp_id2[i];
			if ( pp_id ) {
				for ( j=0 ; j<N_ID_IN_BLOCK ; j++ ) {
					id_cln((struct cap_t *)pp_id[j]) ;
				}
				free((char *)pp_id) ;
			}
		}
		free((char *)pp_id2) ;
		idtable[8]= 0;
	}

	/* Triple indirect */
	pp_id3= (struct cap_t **) idtable[9];
	if ( !pp_id3 ) return ;
	for ( k=0 ; i<N_ID_IN_BLOCK ; k++ ) {
		pp_id2= (struct cap_t **) pp_id3[k];
		if ( pp_id2 ) {
			for ( i=0 ; i<N_ID_IN_BLOCK ; i++ ) {
				pp_id= (struct cap_t **) pp_id2[i];
				if ( pp_id ) {
					for ( j=0 ; j<N_ID_IN_BLOCK ; j++ ) {
						id_cln((struct cap_t *)pp_id[j]) ;
					}
					free((char *)pp_id) ;
				}
			}
			free((char *)pp_id2) ;
		}
		idtable[9]= 0;
		free((char *)pp_id3) ;
	}
}

id_cln(pp_id)
	register struct cap_t	*pp_id ;
{
	/* Cleanup memory used by id table block */
	register struct cap_t	*p_id ;
	int			i,j ;

	if ( !pp_id ) return ;
	for ( i=0 ; i<N_ID_IN_BLOCK ; i++ ) {
		p_id= &pp_id[i] ;
		if ( !cn_type(p_id) ) continue ;
		if ( cn_capsrc(p_id).cs_suite != cn_suite(p_id) ) {
			/* For capset with more then CAPS_IN_ID cap's */
			cs_free(&cn_capsrc(p_id)) ;
		}
		if ( cn_type(p_id)==C_DIR && cn_dir(p_id) ) {
			dir_free(p_id) ;
		}
	}
	free((char *)pp_id) ;

	if ( namestore ) free(namestore) ;
	
}

#endif
