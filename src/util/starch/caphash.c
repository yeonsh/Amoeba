/*	@(#)caphash.c	1.6	96/02/27 13:15:26 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* These hash routines were originally coded by Mike Condict */

#include "starch.h"
#include "caphash.h"

struct obj_table *hash_tab;	/* The main hash_table */

static struct obj_table *obj_table_alloc();
static int obj_eq();

#define MAX_TRIES		20
#define OBJ_EQ(tbl,num,cap2)	(obj_eq((tbl)->objs[num],cap2))
#define UNUSED(tbl,num)		(!(tbl)->objs[num])


/* Returns index of obj in the skip table, or if not found, returns
 * index of unused slot where it may be inserted.  It is an error for the
 * table to be full (and stupid for it to be more than 80% full).
 */
static int
obj_find(skip, cap)
	struct obj_table *skip;
	capability *cap;
{
	register int i,
		     num_tries = 0,
		     size = skip->size;
	register int	jump_factor ;
	int		h;
	objnum		o_num ;
	unsigned char	*cp = (unsigned char *)&cap->cap_port;

	o_num = prv_number(&cap->cap_priv);
	jump_factor = 1 + (o_num) % (size - 1);

	h = (int) (cp[0] * 1051 +	/* Hash the port and objnum */
		   cp[sizeof(port) >> 1] * 521 +
		   cp[sizeof(port) - 1] * 31 +
		   o_num) % skip->size;

	/* Avoid cycles that obviously are not going to examine the whole
	 * table:
	 */
	if (size % jump_factor == 0)
		jump_factor++;

/* fprintf(stderr, "obj_find: --- SIZE %d ---- o_num = %d, h = %d, jump = %d\n", size, o_num, h, jump_factor); */

	/* Attempt to find obj or free slot using ever-increasing jumps: */
	for (i = h; num_tries < MAX_TRIES;
				    i = (i + jump_factor * ++num_tries) % size)
		if (UNUSED(skip, i) || OBJ_EQ(skip, i, cap))
			break;
	if (num_tries < MAX_TRIES) {
/* fprintf(stderr, "obj_find(%d) -> %d (%s)\n", num_tries, i, UNUSED(skip, i) ? "free" : "in use"); */
		return i;
	    }

	if (skip->num_entries >= size) {
/* fprintf(stderr, "obj_find: ***** num_entries (%d) >= size (%d)\n", skip->num_entries, size); */
		return -1;	/* Table full ! */
	}

	/* As last resort, find obj or free slot by scanning forward from
	 * original hashed position:
	 */
	for (i = (h + 1) % size; i != h; i = (i + 1) % size)
		if (UNUSED(skip, i) || OBJ_EQ(skip, i, cap))
			break;
	if (i == h) {
/* fprintf(stderr, "obj_find: ***** table full (i == h) (num_entries = %d size = %d)\n", skip->num_entries, size); */
		return -1;	/* Table full ! */
	}
/* fprintf(stderr, "obj_find(%d) -> %d (%s)\n", num_tries, i, UNUSED(skip, i) ? "free" : "in use"); */
	return i;
}

/* Allocate a hash table to keep track of objects already processed: */
static struct obj_table *
obj_table_alloc(size)
	int size;
{
	register
	struct obj_table *skip = (struct obj_table *)
					malloc(sizeof(struct obj_table));
	if (skip == 0)
		return 0;
	skip->size = size;
	skip->num_entries = 0;
	skip->objs = (long *)
			calloc((unsigned)skip->size,sizeof(long));
	if (skip->objs == (long *)0) {
		free((char *)skip);
		return 0;
	}
	return skip;
}

/* Initialization */
void
init_hash() {
	hash_tab=obj_table_alloc(31) ;
	if ( !hash_tab ) fatal("Out of mem in init_hash");
}


static void
rehash(new_size,min_new_size)
	int		new_size ;
	int		min_new_size ;
{
    struct obj_table	*new_skip ;
    int			try_size ;
    int			slot ;
    int			i ;
    struct cap_t	*p_id ;
    long		tid ;

  
    /* Try to allocate, if not enough memory do binary search between
       new_size and min_new_size until you find some.
       Fail if the distance between min_new_size and last request gets
       lower than HASH_GRANULE.
     */
    /* First free the table that is almost full */
    if ( hash_tab ) free(hash_tab->objs) ;
    /* Then try to get the space for the new table */
    for(try_size=new_size;;) {
	new_skip= obj_table_alloc(try_size);
	if ( new_skip ) break ;
	try_size= (min_new_size/2) + (try_size/2) ; /* Avoid overflow */
	if ( try_size - min_new_size < HASH_GRANULE )
	    fatal("out of memory in rehash") ;
    }

    /* Go through ID table and rehash everything.... */
    for (tid = 2; tid <= max_capid; tid++) {
	    p_id=id_get(tid) ;
	    for ( i = 0; i<cn_capsrc(p_id).cs_final; i++ ) {
		if ( cn_capsrc(p_id).cs_suite[i].s_current ) {
		    slot=obj_find(new_skip,&cn_capsrc(p_id).cs_suite[i].s_object);
		    if (slot < 0 || slot >= new_skip->size) {
			fatal("internal: rehash inconsistency");
		    }
		    new_skip->objs[slot] = tid;
		    ++new_skip->num_entries;
		}
	    }
	    id_unlock(p_id);
    }
    *hash_tab = *new_skip;
    free((ptr)new_skip);
}

/* Insert obj in table pointed to by *hashtable, if not already there.  If table
 * needs to be reallocated, change hashtable->objs to point to the new table and free
 * the old table.  Return flag indicating whether or not entry was already in
 * table:
 */
long
findobj(cap,new,id)
	capability		*cap;
	int			*new;
	long			id;
{
    struct obj_table		*skip;
    register int		slot;

    skip = hash_tab;
    slot  = obj_find(skip, cap);
    if (slot < 0 || slot >= skip->size)
	return 0;	/* Huh? */

    if (UNUSED(skip, slot)) {
	*new = 1;	/* Was not already in table */
	if (skip->num_entries > skip->size * 3 / 4) {
	    /* Reallocate the hash table twice as large: */
	    rehash(2 * (1 + skip->size) - 1, skip->size+10);

	    slot = obj_find(skip, cap);
	    if (slot < 0 || slot >= skip->size)
		return 0;	/* Huh? */
	}
	++skip->num_entries;
	if ( id==0 ) {
		skip->objs[slot] = ++max_capid;
		return max_capid ;
	} else {
		skip->objs[slot] = id;
		return id ;
	}
    } else {
	*new = 0;	/* Was already in table */
	return skip->objs[slot] ;
    }
}

/* Check whether an object is present in the hash table */
long
obj_exists(cap)
	capability		*cap;
{
    struct obj_table		*skip;
    register int		slot;

    skip = hash_tab;
    slot  = obj_find(skip, cap);
    if (slot < 0 || slot >= skip->size)
	return 0;	/* Huh? */

    if (UNUSED(skip, slot)) return 0 ; else return skip->objs[slot] ;
}

/* Rehash the id-table, done if cap's were read from dump description */
void
id_hash() {
    rehash(max_capid + max_capid/5 + HASH_GRANULE, max_capid + 1 ) ;
}

/* End of Condict derived code */

int	max_capid	=	1;	/* Max Cap id used */
int	max_servno 	=	0;	/* Max server number used */

static struct serverlist *
try_server_args(cap)
	capability			*cap ;
{
	register struct serverlist	*slist ;

	/* Check -Z */
	for ( slist=ign_servers ; slist ; slist=slist->next ) {
		if(SAME_SERVER(&slist->server,cap)) {
			return slist ;
		}
	}
	/* Check -Y */
	for ( slist=asis_servers ; slist ; slist=slist->next ) {
		if(SAME_SERVER(&slist->server,cap)) {
			return slist ;
		}
	}
	/* Check -U */
	for ( slist=use_servers ; slist ; slist=slist->next ) {
		if(SAME_SERVER(&slist->server,cap)) {
			return slist ;
		}
	}
	return (struct serverlist *)0 ;
}

static unsigned
get_serv(cap,dump)
	capability			*cap;
	int				dump ;
{
	register struct serverlist	*server ;
	register struct serverlist	*last ;
	register struct serverlist	*slist ;
	int				out_len ;

	last= (struct serverlist *)0 ;
	for ( server=arch_slist ; server ; server=server->next ) {
		/* Try to find the server in the existing list */
		last=server;
		if ( SAME_SERVER(cap,&server->server) ) return server->number ;
		
	}

	/* None found, create new one */
	server=MYMEM(struct serverlist);
	server->server= *cap;
	server->number= ++max_servno ;
	server->next=(struct serverlist *)0 ;
	server->i_type=0 ;
	if ( last ) last->next=server ;
	else	    arch_slist=server ;

	/* The std_info information is only acquired when:
	   -	the contents of the cap are dumped (dir or bullet file)
	   -	the server is the same as the server mentioned in some
		argument list.
	 */

	slist=try_server_args(cap) ;
	if ( slist ) {
		server->i_type= slist->i_type ;
	} else if ( dump ) {
		std_info(cap,&server->i_type,1,&out_len);
	}
	return server->number ;
}

static unsigned
get_cserv(cs,dump)
	capset		*cs ;
	int		dump ;
{
	capability	cap;

	if ( cs_to_cap(cs,&cap)==STD_OK ) return get_serv(&cap,dump) ;
	return 0 ;	
}

static int
obj_eq(id,cap)
	long		id;
	capability	*cap;
{
	register struct cap_t	*p_id;
	register int		i ;
	
	p_id=id_get(id);
	for ( i = 0; i<cn_capsrc(p_id).cs_final; i++ ) {
		if ( cn_capsrc(p_id).cs_suite[i].s_current ) {
		    if (cap_eq(&cn_capsrc(p_id).cs_suite[i].s_object,cap)) {
			id_unlock(p_id);
			return 1 ;
		    }
		}
	}
	id_unlock(p_id);
	return 0 ;
}

long
#ifdef __STDC__
dircap(capability *cap, int *new,struct cap_t **pp_id)
#else
dircap(cap,new,pp_id)
	capability	*cap;
	int		*new;
	struct cap_t	**pp_id;
#endif
{
	long			id;
	register struct cap_t	*p_id;

	id=findobj(cap,new,0L) ;
	p_id=id_get(id);
	*pp_id=p_id ;
	if ( !*new ) {
		id_unlock(p_id) ;
		return id ;
	}
	cs_insert(&cn_capsrc(p_id),cap) ;
	cn_server(p_id)=get_serv(cap,1) ;
	id_unlock(p_id) ;
	return id ;
}

id_collapse(p_sid,did)
	struct cap_t	*p_sid;		/* Pointer to source id */
	long		did;		/* destination id */
{
	struct cap_t	*p_did;		/* Pointer to dest id */
	capset		*scs;		/* Source capset */

	cn_parent(p_sid)=did ;
	cn_flag(p_sid)=LINKED ;
	p_did=id_get(did) ;
	scs= &cn_capsrc(p_sid);
	cs_transfer(&cn_capsrc(p_did),scs);
	cs_reset(scs) ;
	id_unlock(p_did) ;
}
	
	

long
#ifdef __STDC__
findcs(capset *cs, int res, long pid)
#else
findcs(cs,res,pid)
	capset		*cs;
	int		res;
	long		pid;
#endif
{
	int			new;
	int			first=1;
	int			id_inited ;
	long			id;
	long			tid;
	register struct cap_t	*p_id;
	register struct cap_t	*p_tid;
	register int		i ;

	for ( i = 0; i<cs->cs_final; i++ ) if ( cs->cs_suite[i].s_current ) {
		if ( first ) {
			id=findobj(&cs->cs_suite[i].s_object,&new,0L);
			p_id=id_get(id) ;
			if ( new ) {
				cs_insert(&cn_capsrc(p_id),
						&cs->cs_suite[i].s_object);
				id_inited=0 ;
			} else {
				id_inited=1 ;
			}
			first=0 ;
		} else {
			tid=findobj(&cs->cs_suite[i].s_object,&new,id);
			if ( tid!=id ) { /* OK, found a link */
				if ( !id_inited ) {
					/* Currently have master id */
					cn_parent(p_id)=tid ;
					cn_flag(p_id)=LINKED ;
					id_unlock(p_id) ;
					id_collapse(p_id,tid) ;
					id=tid ;
					p_id=id_get(id) ;
					id_inited=1 ;
				} else {
					p_tid=id_get(tid) ;
					id_collapse(p_tid,id) ;
					id_unlock(p_tid) ;
				}
			} else {
				if ( new ) {
					cs_insert(&cn_capsrc(p_id),
						&cs->cs_suite[i].s_object) ;
				}
			}	
		}
	}
	if ( first ) /* Empty suite, simply */ return 0 ;
	if ( id_inited ) return id ;
	cn_flag(p_id)=res ;
	if ( res&M_CONTENT ) {
		cn_type(p_id)=C_FILE ;
		cn_server(p_id)=get_cserv(cs,1) ;
	} else {
		cn_type(p_id)=C_OTHER;
		cn_server(p_id)=get_cserv(cs,0) ;
	}
	cn_parent(p_id)=pid;
	id_unlock(p_id) ;
	return id ;
}

hash_cleanup() {
	/* Free all memory used for hash table */
	if ( hash_tab ) {
		free( (char *)hash_tab->objs ) ;
		free( (char *)hash_tab ) ;
	}
}
