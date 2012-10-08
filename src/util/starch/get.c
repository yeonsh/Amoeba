/*	@(#)get.c	1.7	96/02/27 13:15:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* STARCH - STore and reSTore ARCHives of graphs.
   module implementing -i option


*/

#include <signal.h>
#include "starch.h"
#include "block.h"
#include <module/path.h>

#ifdef MULTI_THREADED
#define TTY_LOCK	err_lock();
#define TTY_UNLOCK	err_unlock();
#else
#define TTY_LOCK	;
#define TTY_UNLOCK	;
#endif

extern struct ba_list	*r_io ;		/* readblock and rio_* admin data */

/* A set of macro mapping rbl_* into wio*, using r_io */
#define	rbl_string()	rio_string(r_io)
#define rbl_mem(n)	rio_mem(r_io,(n))
#define rbl_4b()	rio_4b(r_io)
#define rbl_2b()	rio_2b(r_io)
#define rbl_1b()	rio_1b(r_io)

/* Local definition of exported variables */
int			do_list ;	/* List i.s.o. extract */

/* Local variables */
static	int		partial ;	/* Partial extract */

static int		unreach_count ;	/* Used by set_entry_point */
static capset		unreach_dir_cs ;/* Used by install_unreach */
static int		have_unreach;	/* Indicates presence of unrachables */


static long		cur_dir_id ;	/* Dir id reading currently */
static int		cur_dir_cnt ;	/* # entries read into above */
static long		max_dir_id ;	/* Max dir id read */

/* Stuff for get_datasize, start_getdat, skip_data, get_data */
static long		cur_data_id ;	/* Id of current input */
static struct cap_t	*cur_data_p_id ;/* P_id of current input */
static long		skip_id ;	/* Id to be skipped for getdata */
static long		cur_data_size ; /* Size expected to read */
static long		cur_data_off ;	/* Currently reading at off */
static int		cur_data_first_size ; /* Size of first block */
static void		(*cur_data_handler)() ; /* Handler */
static int		data_read ;	/* Tells whether rbl_mem is called */

/* Stuff for ass_file */
static capability	cur_file_cap;	/* Cap. currently writing into*/
static int		cur_file_stat;	/* Cap. status */
		/* 0=> not open yet, 1=> open, 2=> closed */

/* Stuff for ass_cap */
static capability	ass_new_cap;	/* Cap to be assembled */
static int		cap_ok;		/* Cap is assembled */

/* General stuff */
static capset		cs_bullet ;	/* bullet capset */
static capability	bullet_server ;	/* bullet server cap */
static capset		dir_server ;	/* directory server cap */
static capset		*dir_cs ;	/* directory server cap */
static int		ok_bullet=0 ;	/* flag for validity of above */

int			nasty_one ;	/* Ahh, nasty path's in id #1 */

/* local routines */
#ifdef __STDC__
static struct dir_ent	*path_unravel(char *path,int extract,
						struct cap_t **pp_dirid);
static void		install_unreach(long id,struct cap_t *p_id);
#else
static struct dir_ent	*path_unravel();
static void		install_unreach();
#define const		/**/
#endif
static int		better_rights();
static void		ass_cap();
static void		ass_file();
static void		skip_data();
static void		clear_dir_loops();
void			get_graph();
void			get_select();
void			cre_dirs();
void			chk_e_visit();
void			chk_exist();
void			chk_dots();
void			check_last();
void			def_server();
void			get_contents();
void			cre_entries();
void			list_dirs();


int
dolist()
{
	do_list = 1 ;
	return doget() ;
}

int
doget()
{
	errstat		errcode;
	int		result;

	init_id();

	/* Some initial preparations */
	partial = ( files || xlist || xxlist || in_multi) ;
	/* Try to access the archive */
	openarch(archive,0,0,0) ;
	if ( !do_stdio ) get_disksize(1) ;
	init_read();
	/* Read in the graph info */
	get_graph();
	if ( partial ) get_select() ;
	if ( do_list ) {
		list_dirs() ;
		if ( verbose>0 ) get_contents() ;
	} else {
		if ( retain ) {
			chk_exist() ;
		} else {
			chk_dots() ;
		}
		if ( dir_servers ) {
			if ( dir_servers->next ) {
				warning("only using first -D flag at the moment");
			}
			result=cs_singleton(&dir_server,&dir_servers->server);
			if ( !result ) {
				fatal("cs_singleton failed");
			}
			dir_cs = &dir_server ;
		} else {
			dir_cs= SP_DEFAULT ;
		}
		signal(SIGHUP,killed) ;
		signal(SIGINT,killed) ;
		signal(SIGBUS,killed) ;
		signal(SIGSEGV,killed) ;
		signal(SIGSYS,killed) ;
		signal(SIGTERM,killed) ;
		cre_dirs() ;
#ifndef NOTDEF
		/* This was an attempt to check access before reading
		   the rest of the tape. It failed. All entry creation
		   is now done in cre_entries & check_last
		 */
		nasty_one=check_access() ;
#else
		nasty_one=1 ;
#endif
		get_contents() ;
	}
	if ( io_type==IO_TAPE ) {
		/* The reader thread must be finished by now */
		errcode=tape_rewind(&arch_cap,0) ;
		if (errcode!=STD_OK ) {
			warning("could not rewind tape") ;
		}
	}
	if ( stats ) iostats() ;
	if ( !do_list ) {
		cre_entries() ;
		if ( have_unreach ) {
			if ( unreach_path ) {
				cre_unreach() ;
			} else {
				warning("need -k flag to extract unreachable files") ;
				del_unreach() ;
			}
		}
		if ( nasty_one ) check_last();
	}
#ifdef MEMDEBUG
	get_cleanup() ;
#endif
	return 1 ;
}

static void dirdef(cont) 
	int	cont;
{
	long			id ;
	register struct cap_t	*p_id ;
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register char		**pp_c ;
	register long		*p_rights ;
	register int		i,j ;
	int			count ;

	id=rbl_4b() ;
	if ( id>max_capid ) max_capid=id ;
	if ( cont ) {
		if ( id!=cur_dir_id ) {
			warning("missing definition for directory # %d",
				id) ;
			return ;
		}
		p_id=id_get(id) ;
		p_dl=cn_dir(p_id) ;
		p_de=p_dl->r_entries+cur_dir_cnt ;
	} else {
		if ( id>max_dir_id ) max_dir_id=id ;
		p_id=id_get(id) ;
		cn_type(p_id)=C_DIR;
		cn_server(p_id)=rbl_2b() ;
		/* ignore the date */
		rbl_4b();
		p_dl=MYMEM(struct dirlist);
		cn_dir(p_id)=p_dl;
		/* get version */
		p_dl->d_version.seq[0]=rbl_4b();
		p_dl->d_version.seq[1]=rbl_4b();
		/* get column count */
		p_dl->c_count=rbl_2b();
		if ( p_dl->c_count ) {
			pp_c= (char **)getmem(p_dl->c_count*sizeof(char *));
		} else {
			pp_c= (char **)0;
		}
		p_dl->c_names=pp_c ;
		for ( i=0 ; i<p_dl->c_count ; i++ ) {
			pp_c[i]= keepstr(rbl_string()) ;
		}
		p_dl->r_count=rbl_4b();
		if ( p_dl->r_count>0 ) {
			p_de= (struct dir_ent *)getmem(p_dl->r_count*sizeof(struct dir_ent));
			p_dl->r_entries=p_de ;
		} else {
			p_dl->r_entries= (struct dir_ent *)0 ;
		}
		cur_dir_cnt=0 ;
	}
	count=rbl_2b() ;
	if ( cur_dir_cnt+count > p_dl->r_count ) {
		warning("inconsistency in directory count for id %ld, %d entries skipped",
		id,cur_dir_cnt+count-p_dl->r_count) ;
		count=p_dl->r_count-cur_dir_cnt ;
	}
	for ( i=0 ; i<count ; i++, p_de++ ) {
		p_de->name= keepstr(rbl_string()) ;
		p_de->time= rbl_4b() ;
		/* ignore the other two dates */
		rbl_4b(); rbl_4b();
		p_rights= (long *)getmem(p_dl->c_count*sizeof(long));
		for ( j=0 ; j<p_dl->c_count ; j++ ) {
			p_rights[j]= rbl_4b() ;
		}
		p_de->rights= p_rights ;
		p_de->capno= rbl_4b() ;
		if ( acq_cap(p_de) >max_capid ) max_capid=acq_cap(p_de) ;
		p_de->caprights= rbl_4b() ;
		p_de->flags= ENT_VALID ;
		
	}
	id_unlock(p_id) ;
	cur_dir_id=id ;		/* Dir cap reading currently */
	cur_dir_cnt += count ;	/* # entries read into above */
}

int path_depth(id)
	long		id;
{
	register struct cap_t	*p_id ;
	long			papa ;

	if ( id==1 ) return 0 ;
	
	p_id=id_get(id) ;
	papa=cn_parent(p_id) ;
	if ( papa==0 ) return 1000 ;
	id_unlock(p_id) ;
	return path_depth(papa)+1 ;
}
	
	
static void aai(id,depth)
	long		id ;
	int		depth ;
{
	register struct cap_t	*p_id ;
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register int		i ;
	register struct cap_t	*p_id2 ;

	p_id=id_get(id) ;
	p_dl=cn_dir(p_id) ;
	if ( !p_dl ) return ;
	p_de=p_dl->r_entries ;
	if ( !p_de ) return ;
	for ( i=0 ; i<p_dl->r_count ; i++, p_de++ ) {
		if ( !(p_de->flags&ENT_VALID) ) continue ;
		p_id2=id_get(acq_cap(p_de));
		if ( cn_parent(p_id2)==0 ||
		     depth<path_depth(cn_parent(p_id2))
		) {
			cn_parent(p_id2)=id ;
			if ( cn_type(p_id2)==C_DIR ) {
				/* recursive */
				aai(acq_cap(p_de),depth+1) ;
			}
		}
		id_unlock(p_id2) ;
	}
	id_unlock(p_id) ;
}

static void set_parents() {
	/* Go through the dir structure and set the parent field */
	register long		id ;
	register struct cap_t	*p_id ;
	int			unreach ;

	p_id=id_get(1L);
	if ( cn_type(p_id)==C_DIR ) {
		aai(1L,0);
	}
	id_unlock(p_id) ;
	unreach=0 ;
	for ( id=2 ; id<=max_dir_id ; id++ ) {
		p_id=id_get(id) ;
		if ( cn_type(p_id)==C_DIR && cn_parent(p_id)==0 ) {
			unreach=1 ;
			if ( unreach_path ) aai(id,1000);
		}
		id_unlock(p_id) ;
	}
	if ( unreach ) {
		/* There were unreachable directories */
		have_unreach=1 ;
	}

}

void
get_graph()
{
	int		type ;
	int		subtype ;

	for (;;) {
		type=read_block() ;
		switch(type) {
		case -1:
			return ;
		case AR_GRAPH:
			break ;
		case AR_CONTENT:
			subtype=rbl_1b() ;
			switch(subtype) {
			case AR_SERVER:
				def_server() ; break ;
			default:
				warning("unexpected content subtype, block skipped") ;
				break ;
			}
			continue ;
		default:
			warning("unexpected block type, block skipped") ;
			continue ;
		}
		/* Only here when type==AR_GRAPH */
		subtype=rbl_1b() ;
		switch(subtype) {
		case AR_DIRDEF:
			dirdef(0) ; break ;
		case AR_DIRCONT:
			dirdef(1) ; break ;
		case AR_DIREND:
			set_parents() ;
			return ;
		default:
			warning("unexpected graph subtype, block skipped") ;
			continue ;
		}
	}

}

void
set_ignore(path)
	char			*path ;
{
	register struct cap_t	*p_id ;
	struct dir_ent		*p_de ;
	struct cap_t		*p_dirid ;

	/* Find id */
	p_de=path_unravel(path,0,&p_dirid);
	if ( !p_de ) {
		warning("can not find path \"%s\"",path) ;
		return ;
	}

	p_id=id_get(acq_cap(p_de)) ;
	cn_flag(p_id)|=M_IGNORE ;
	id_unlock(p_id) ;
	id_unlock(p_dirid) ;
	
}

void
set_extract(id)
	long		id;
{
	register struct cap_t	*p_id ;
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register int		i ;
	register struct patternlist	*plist ;
	int			ignore ;

	p_id=id_get(id) ;
	if ( cn_flag(p_id)&(LOOPED|M_IGNORE) ) {
		id_unlock(p_id) ;
		return ;
	}
	cn_flag(p_id)|= (LOOPED|EXTRACT) ;
	if ( cn_type(p_id)!=C_DIR ) {
		id_unlock(p_id) ;
		return ;
	}
	p_dl=cn_dir(p_id) ;
	if ( !p_dl ) {
		id_unlock(p_id) ;
		return ;
	}
	p_de=p_dl->r_entries ;
	for ( i=0 ; i<p_dl->r_count ; i++, p_de++ ) {
		if ( !(p_de->flags&ENT_VALID) ) continue ;
		ignore=0 ;
		for ( plist=xxlist ; plist ; plist=plist->next ) {
			if (fnmatch(plist->pattern,p_de->name,0)) {
				ignore=1 ;
				break ;
			}
		}
		if ( ignore ) continue ;
		p_de->flags |= ENT_EXTRACT ;
		set_extract(acq_cap(p_de)) ;
	}
	id_unlock(p_id) ;
}

void
get_select() {
	/* Now we have a partial extract, the procedure is as follows:
	   first set M_IGNORE for every capability indicated by -x flags.
	   Then visit the graph recursively through all file name
	   arguments, while skipping names fitting the -X criterium.
	   The EXTRACT flag is used to indicate that an id has to be
	   extracted.
	   The LOOPED value is used, to indicate
	   that that id has already recursively been checked for
	   id's to extract.
	 */
	register struct	pathlist	*flist ;
	register struct dir_ent		*p_de ;
	struct cap_t			*p_dirid ;

	/* Check -x */
	for ( flist=xlist ; flist ; flist=flist->next ) {
		set_ignore(flist->path) ;
	}
	if ( !files ) { /* No pathname args */
		set_extract(1L);
	} else {
		for ( flist=files ; flist ; flist=flist->next ) {
                	p_de=path_unravel(flist->path,1,&p_dirid);
			if ( !p_de ) {
				warning("can not find path \"%s\" in archive",
						flist->path) ;
			} else {
				set_extract(acq_cap(p_de)) ;
				id_unlock(p_dirid) ;
			}
        	}
	}
}

char *inipart(head,all)
	char		*head;
	char		*all;
{
	/* Is head the initial part of all. Path name criteria
	   are used for initial, that is thing have to match to a 0 or '/'.
	 */
	register char *sh, *sa ;

	sh=head, sa=all ;
	if ( (*sh=='/' && *sa!='/') || (*sa=='/' && *sh!='/') ) {
		return (char *)0 ;
	}
	/* skip initial slashes */
	while ( *sh=='/' ) sh++ ;
	while ( *sa=='/' ) sa++ ;
	/* Start comparing */
	for(;;) {
		if ( *sa=='/' || *sa==0 ) {
			while ( *sa=='/' ) sa++ ; /* multiple slashes */
			if ( *sh==0 || *sh=='/' ) {
				/* (Partial) match */
				while ( *sh=='/' ) sh++ ; /* mult. slashes */
				if ( *sh==0 ) return sa ;
				if ( *sa==0 ) {
					/* Curious case, dump command was:
					   starch -o xxx /usr/share/local
					   extract command is like:
					   starch -i xxx /usr/share
					   I will extract.
					 */
					return sa ;
				}
				continue ;
			} /* No fall though */
			return (char *)0 ;
		}
		if ( *sa!=*sh ) return (char *)0 ;
		sa++, sh++;
	}
	/* UNREACHED */
}

struct dir_ent *x_path_un(id,path,f,pp_dirid)
	long	id;
	char	*path;
	int	f;
	struct cap_t **pp_dirid;
{
	register struct cap_t	*p_id ;
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register int		i ;
	char			*tail ;
	struct dir_ent		*ret_id ;
	int			retain_lock ;

	p_id=id_get(id) ;
	p_dl=cn_dir(p_id) ;
	if ( cn_type(p_id)!=C_DIR || !p_dl ) {
		id_unlock(p_id) ;
		return 0 ;
	}
	p_de=p_dl->r_entries ;
	ret_id=0 ;
	retain_lock=0 ;
	for ( i=0 ; i<p_dl->r_count ; i++, p_de++ ) {
		if ( !(p_de->flags&ENT_VALID) ) continue ;
		if ( tail=inipart(p_de->name,path) ) {
			if ( *tail==0 ) {
				*pp_dirid= p_id ;
				retain_lock=1;
				ret_id=p_de ;
			} else {
				/* If non-zero look for rest */
				ret_id=x_path_un(acq_cap(p_de),tail,f,pp_dirid) ;
			}
			/* Only entry one can contain (partial)
			   identical entries
			 */
			if ( f && ret_id ) p_de->flags |= ENT_EXTRACT ;
			if ( id!=1L ) break ;
		}
	}
	if ( f && ret_id ) {
		cn_flag(p_id) |= EXTRACT ;
	}
	if ( !retain_lock ) id_unlock(p_id) ;
	return ret_id ;
}
	
static struct dir_ent *
#ifdef __STDC__
path_unravel(char *path,int extract,struct cap_t **pp_dirid)
#else
path_unravel(path,extract,pp_dirid)
	char	*path;
	int	extract;
	struct cap_t **pp_dirid;
#endif
{
	return x_path_un(1L,path,extract,pp_dirid);	
}

void
cre_dirs() {
	/* Create all directories not already valid */
	register long		id ;
	register struct cap_t	*p_id ;
	register struct dirlist	*p_dl ;
	char			**clmns ;
	int			clmncnt=10 ;
	register char		**ncs, **ncd ;
	register int		i ;
	errstat			err ;
	capset			dircs ;

	clmns=(char **)getmem((unsigned)(clmncnt*sizeof(char**))) ;
	for ( id=2 ; id<=max_dir_id ; id++ ) {
		p_id=id_get(id) ;
		if ( (!partial || cn_flag(p_id)&EXTRACT) &&
		     cn_type(p_id)==C_DIR && !(cn_flag(p_id)&VALID) ) {
			p_dl=cn_dir(p_id) ;
			if ( p_dl->c_count>=clmncnt ) {
				clmncnt=p_dl->c_count+10 ;
				clmns=(char**)adjmem(clmns,
					(unsigned)(clmncnt*sizeof(char**)));
			}
			/* Copy columns to right format */
			for ( i=0,ncd=clmns,ncs=p_dl->c_names ;
			      i<p_dl->c_count ;
			      i++ ) {
				*ncd++ = *ncs++ ;
			}
			*ncd= (char *)0 ;
			err=sp_create(dir_cs,clmns,&dircs);
			if ( err!=STD_OK ) {
				warning("during the creation of directories");
				soap_err(err);
			} else {
				cs_transfer(&cn_capdst(p_id),&dircs);
				cn_flag(p_id) |= (VALID|CREATED) ;
				cs_free(&dircs);
			}
		}
		id_unlock(p_id) ;
	}
	free((char *)clmns) ;
}

void
set_exists(p_de,cs,dots)
	struct dir_ent		*p_de ;
	capset			*cs;
	int			dots ;
{
	register struct cap_t	*p_id ;
	long			found_some ;

	found_some=0 ;
	p_id=id_get(acq_cap(p_de));
	if ( cn_flag(p_id)&EXISTS ) {
		if ( !cs_overlap(&cn_capdst(p_id),cs) ) {
			found_some=acq_cap(p_de);
		} else {
			if ( better_rights(&cn_capdst(p_id),cs) ) {
				cs_reset(&cn_capdst(p_id));
				cs_transfer(&cn_capdst(p_id),cs);
				if ( cn_type(p_id)==C_DIR) {
					chk_e_visit(p_id,cs,dots) ;
				}	
			}
		}
	}
	if ( found_some ) {
warning("linking of existing %s does not seem to conform");
warning("to linking in the archive of the same file", id_pn(found_some));
	} else { 
		/* found it */
		p_de->flags |= ENT_EXISTS ;
		cs_transfer(&cn_capdst(p_id),cs);
		cn_flag(p_id) |= (EXISTS|VALID) ;
		if ( cn_type(p_id)==C_DIR ){
			/* recursive */
			chk_e_visit(p_id,cs,dots) ;
		}
	}
	id_unlock(p_id);
}

void
chk_e_visit(p_id,dircs,dots)
	struct cap_t	*p_id ;
	capset		*dircs ;
	int		dots ; /* check only "" and "." */
{		
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register int		i ;
	errstat			err;
	capset			ex_cs ;
	char			*path_name ;

	p_dl=cn_dir(p_id) ;
	if ( !p_dl ) return ;
	p_de=p_dl->r_entries ;
	for ( i=0 ; i<p_dl->r_count ; i++, p_de++ ) {
		if ( !p_de->flags&ENT_VALID ) continue ;
		if ( !partial || (p_de->flags&ENT_EXTRACT) ) {
			if ( dots ) {
				if ( *p_de->name==0 ||
				     strcmp(p_de->name,".")==0 ) {
					path_name="" ;
				} else {
					continue ;
				}
			} else {
				path_name=p_de->name ;
			}
			err=sp_lookup(dircs,path_name,&ex_cs);
			if ( err==STD_OK ) {
				set_exists(p_de,&ex_cs,dots) ;
				cs_free(&ex_cs);
			}
		}
	}
}

void
chk_exist() {
	register struct cap_t	*p_id ;

	p_id=id_get(1L) ;
	if ( cn_type(p_id)!=C_DIR ) {
		id_unlock(p_id) ;
		return ;
	}
#ifdef NOTDEF
/* Alas, SP_DEFAULT proved to be 0 */
	cs_transfer(&cn_capdst(p_id),SP_DEFAULT);
#endif
	cn_flag(p_id) |= EXISTS ;
	chk_e_visit(p_id,SP_DEFAULT,0) ;
	id_unlock(p_id) ;
}

void
chk_dots() {
	register struct cap_t	*p_id ;

	p_id=id_get(1L) ;
	if ( cn_type(p_id)!=C_DIR ) {
		id_unlock(p_id) ;
		return ;
	}
#ifdef NOTDEF
/* Alas, SP_DEFAULT proved to be 0 */
	cs_transfer(&cn_capdst(p_id),SP_DEFAULT);
#endif
	cn_flag(p_id) |= EXISTS ;
	chk_e_visit(p_id,SP_DEFAULT,1) ;
	id_unlock(p_id) ;
}

int dw_call(p_id,proc,must_flag)
	struct cap_t		*p_id ;
	int			(*proc)();
	int			must_flag ;
{
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register struct dir_ent	*p_de2 ;
	int			skipped=0 ;
	int			skip_it ;
	register int		i, j ;
	char			*tail ;

	p_dl=cn_dir(p_id) ;
	if ( !p_dl ) {
		warning("internal: missing directory information") ;
		return 0 ;
	}
	p_de=p_dl->r_entries ;
	for ( i=0 ; i<p_dl->r_count ; i++, p_de++ ) {
		if ( p_de->flags&(ENT_DONE|ENT_EXISTS) ) continue ;
		if ( !(p_de->flags&ENT_VALID) ) continue ;
		if ( partial && !(p_de->flags&ENT_EXTRACT) ) continue ;
		p_de2=p_dl->r_entries ;
		skip_it=0 ;
		for (j=0;j<p_dl->r_count;j++,p_de2++) {
			if ( i==j ) continue ;
			if ( p_de2->flags&(ENT_DONE|ENT_EXISTS) ) continue ;
			if ( !(p_de2->flags&ENT_VALID) ) continue ;
			if ( partial && !(p_de2->flags&ENT_EXTRACT) ) continue ;
			tail=inipart(p_de2->name,p_de->name);
			if ( tail ) {
				if ( *tail==0 ) {
					/* identical access names,
					   set this one to done
					 */
					p_de2->flags |= ENT_DONE ;
				} else {
					/* Skip acting on this file */
					skip_it=1 ;
					break ;
				}
			}
		}
		if ( skip_it ) {
			skipped++ ;
		} else {
			if ( (*proc)(p_de,must_flag) ) {
				p_de->flags |= ENT_DONE ;
			} else {
				skipped++ ;
			}		
		}
	}
	return skipped ;
}


int cre_path(p_de,must_flag)
	struct dir_ent		*p_de;
	int			must_flag;
{
	/* Create the main entries into the structure to be extracted */
	register struct cap_t	*p_id ;
	register char		*tail ;
	char			*end ;
	capset			*curdir ;
	capset			*nextdir ;
	capset			*tmpdir ;
	capset			csdir1 ;
	capset			csdir2 ;
	capability		cap ;
	char			lastval ;
	errstat			err ;
	int			overwrite ;
	char			*pathname ;
	int			pathlength ;
	char			*cwd ;
	extern char		*getenv() ;

	if ( p_de->flags&ENT_EXISTS ) {
		if ( verbose ) {
			message("retaining %s",p_de->name);
		}
		return 1 ;
	}

	curdir= SP_DEFAULT ;
	nextdir= &csdir2 ;
	/* Allocate space for the name & normalize it */
	pathlength=strlen(p_de->name) ;
	pathname= (char *)getmem(pathlength+1) ;
	cwd=getenv("_WORK");
	if ( path_rnorm(cwd,p_de->name,pathname,pathlength)== -1 ) {
		fatal("can not access %s (too many ..'s ?)",p_de->name);
	}
	tail=pathname ;
	do {
		end=tail ;
		/* tail points to remaining path */
		/* search for / or end-of-string */
		if ( tail==pathname ) {
			/* skip initial '/' */
			while ( *end=='/' ) end++ ;
		}
		/* search for / or end-of-string */
		while ( !(*end==0 || *end=='/') ) end++ ;
		lastval= *end ; *end=0 ; if ( lastval) end++ ;
scream:
		overwrite=0 ;
		err=sp_lookup(curdir,tail,nextdir) ;
		if ( !(err==STD_NOTFOUND || err==STD_OK )) {
			if ( err==STD_DENIED ) fatal("can not lookup %s",
							p_de->name) ;
			soap_err(err) ;
			if ( curdir) cs_free(curdir);
			free(pathname);
			return 0 ;
		}
		if ( err==STD_OK && lastval==0 ) {
			/* We want to install our own, if the dir existed
			   during chk_exists we would not be here.
			   So remove this if -f allows it.
			 */

			if ( !force ) {
				fatal("%s already exists",p_de->name) ;
			}
			cs_free(nextdir);
			overwrite=1 ; /* Act as if never existed */
		}
		if ( err==STD_NOTFOUND || overwrite ) {
			/* Create/Install a new dir */
			if ( !lastval ) {
				p_id=id_get(acq_cap(p_de)) ;
				if ( !(cn_flag(p_id)&VALID) ) {
					if ( must_flag ) {
fatal("can not create main entry point %s",id_pn(acq_cap(p_de))) ;
					} else {
						id_unlock(p_id);
						if ( curdir ) cs_free(curdir) ;
						free(pathname) ;
						return 0 ;
					}
				}
				if ( !cs_copy(nextdir,&cn_capdst(p_id)) ) {
					fatal("internal, cs_copy failed") ;
				}
				id_unlock(p_id) ;
			} else {
				err=sp_create(dir_cs,sts_columns,nextdir) ;
				if ( err!=STD_OK ) {
					soap_err(err) ;
					if ( curdir ) cs_free(curdir) ;
					free(pathname);
					return 0 ;
				}
			}
			/* Now try to install */
			if ( overwrite ) {
				err=sp_replace(curdir,tail,nextdir) ;
			} else {
				err=sp_append(curdir,tail,nextdir,sts_ncols,sts_rights) ;
			}
			if ( err==STD_EXISTS ) {
				if ( !lastval && cs_to_cap(nextdir,&cap)==STD_OK) {
					
					std_destroy(&cap) ;
				}
				cs_free(nextdir);
				goto scream ;
			}
			if ( err==STD_NOTFOUND ) {
				fatal("path %s disappeared",p_de->name) ;
			}
			if ( err!=STD_OK ) {
				if ( err==STD_DENIED ) {
fatal("no permission to create %s",p_de->name) ;
				}
				warning("could not create %s",p_de->name) ;
				cs_free(nextdir);
				soap_err(err) ;
				if ( curdir ) cs_free(curdir) ;
				free(pathname);
				return 0 ;
			}
			if ( lastval==0 && verbose ) {
				if ( overwrite ) {
					message("replaced %s",p_de->name);
				} else {
					message("installed %s",p_de->name);
				}
			}
		}
		if ( curdir ) {
			cs_free(curdir) ;
		} else {
			/* Fake pointing to one whole time */
			curdir= &csdir1 ;
		}
		/* exchange the pointers */
		tmpdir=curdir ; curdir=nextdir ; nextdir=tmpdir ;
		tail=end;
	} while ( *tail  );
	cs_free(curdir) ;
	free(pathname);
	return 1 ;
}

int
check_access() {
	/* Creating this code was painfull */
	/* That really hard part is when the archive contains
	   the description for src/id/id2/... and src but not
	   for src/id. The trick I use is
		1) In checking for selection, to check all paths
		   through id #1;
		2) Split the checking for access in two parts
			a) this routine that tries to install
			   the main entries to the graph like
			   src, but not src/id.
			b) if all of the archive has been retrieved
			   and a) did skipped a few, try a) again
			   and again and again.
	*/
	struct cap_t		*p_id ;
	int			retval ;

	p_id=id_get(1L) ;
	retval=0 ;
	if ( cn_type(p_id)==C_DIR ) {
		retval=dw_call(p_id,cre_path,0);
	}
	id_unlock(p_id) ;
	return retval ;
}

void
check_last() {
	/* Creating this code was painfull,
	   see the comment in check_access.
	*/
	struct cap_t		*p_id ;
	int			n_to_do ;
	int			prev_n_to_do ;

	p_id=id_get(1L) ;
	if ( cn_type(p_id)!=C_DIR ) {
		id_unlock(p_id) ;
		return ;
	}
	prev_n_to_do=0 ;
	do {
		n_to_do=dw_call(p_id,cre_path,1) ;
		if ( prev_n_to_do && prev_n_to_do==n_to_do ) {
			fatal("internal failure: could not install entry points");
		}
		prev_n_to_do=n_to_do ;
	} while ( n_to_do!=0 ) ;
	id_unlock(p_id) ;
}

void
def_server() {
	if ( do_list ) return ;
	/* Empty for now */
}

void
init_bullet() {
	errstat		err;

	if ( file_servers ) {
		if ( file_servers->next ) {
			warning("currently using only first -B parameter");
		}
		bullet_server= file_servers->server ;
	} else {
		err=sp_lookup(SP_DEFAULT,BULLET_DEFAULT,&cs_bullet) ;
		if ( err!=STD_OK ) {
			fatal("could not find bullet capability") ;
		}
		err=cs_to_cap(&cs_bullet,&bullet_server) ;
		if ( err!=STD_OK ) {
			fatal("could not find a good bullet capability") ;
		}
		cs_free(&cs_bullet) ;
	}
	ok_bullet=1 ;
}

void
cur_close(){

	(*cur_data_handler)(0L,-1,(ptr)0) ;

	skip_data() ;
}

static void
ass_file(cur_off,buf_len,buf_ptr)
	long		cur_off;
	int		buf_len;
	ptr		buf_ptr ;
{
	int		create=0 ;
	int		commit=0 ;
	errstat		err ;
	capability	prev_cap ;

	if ( buf_len<=0 && cur_file_stat==1 ) {
		/* Prematurely close the file */
		prev_cap=cur_file_cap ;
		for (;;) {
			err=b_modify(&prev_cap,(b_fsize)0,(char *)0,
				(b_fsize)0,BS_COMMIT,&cur_file_cap);
			if ( err!=STD_NOMEM ) break ;
			warning("bullet server is out of memory") ;
			sleep(350);
		}
		if ( err!=STD_OK ) {
			bullet_err(err) ;
			std_destroy(&prev_cap) ;
		} else {
			cs_insert(&cn_capdst(cur_data_p_id),&cur_file_cap);
			cn_flag(cur_data_p_id) |= (VALID|CREATED) ;
		}
		cur_file_stat=0 ;
		return ;
	}
	if ( buf_len==0 ) {
		/* End-of-file, should already have happened */
		if ( cur_file_stat!=2 ) {
			if ( cur_file_stat==1 ) {
				warning("prematute EOF on %s",
						id_pn(cur_data_id));
			} else {
				warning("internal inconsistency, double EOF") ;
			}
		}
		cur_file_stat=0 ;
		return ;
	}
	if ( buf_len<0 ) {
		/* Error in archive, try to save all */
		switch ( cur_file_stat ) {
		case 0 :
			warning("archive error, not restored %s",
					id_pn(cur_data_id));
			break ;
		case 1 :
			warning("archive error, partially restored %s",
					id_pn(cur_data_id));
			break ;
		case 2 :
			warning("internal error while processing %s",
					id_pn(cur_data_id));
			break ;
		}
		cur_file_stat=0 ;
		return ;
	}
	if ( cur_file_stat==0 ) create=1 ;
	if ( cur_off+buf_len>=cur_data_size ) commit=BS_COMMIT ;
	if ( !ok_bullet ) init_bullet() ;
	if ( create ) {
		err=b_create(&bullet_server,buf_ptr,(b_fsize)buf_len,commit,
				&cur_file_cap) ;
	} else {
		prev_cap=cur_file_cap ;
		for (;;) {
			err=b_modify(&prev_cap,(b_fsize)cur_off,
				(char *)buf_ptr,
				(b_fsize)buf_len,commit,&cur_file_cap);
			if ( err!=STD_NOMEM ) break ;
			warning("bullet server is out of memory");
			sleep(350);
		}
	}
	if ( err!=STD_OK ) {
		if ( !create ) std_destroy(&prev_cap) ;
		warning("could not write %s",id_pn(cur_data_id));
		bullet_err(err) ;
		skip_data();
		return ;
	}
	if ( create ) cur_file_stat=1 ;
	if ( commit ) {
		cs_insert(&cn_capdst(cur_data_p_id),&cur_file_cap);
		cn_flag(cur_data_p_id) |= (VALID|CREATED) ;
		cur_file_stat=2 ;
	}
}

static void
ass_cap(cur_off,buf_len,buf_ptr)
	long		cur_off;
	int		buf_len;
	ptr		buf_ptr ;
{

	if ( buf_len<0 ) {
		/* Abort assembly */
		cap_ok=0 ;
		return ;
	}
	if ( buf_len==0 ) {
		/* End-of-data, should already have happened */
		if ( cap_ok ) {
			if ( do_list && verbose>1 ) {
				printf("            %s\n",ar_cap(&ass_new_cap));
			} else {
				cs_insert(&cn_capdst(cur_data_p_id),
						&ass_new_cap);
				cn_flag(cur_data_p_id) |= VALID ;
			}
		} else {
			message("failed to assemble capability for %s",
				id_pn(cur_data_id));
		}
		cap_ok=0 ;
		return ;
	}
	memcpy((char *)&ass_new_cap + cur_off, buf_ptr, buf_len) ;
	if ( cur_off+buf_len==CAP_SZ ) cap_ok=1 ;
}

long
get_datasize(id,p_id)
	long		id ;
	struct cap_t	*p_id ;
{
	int		firstbyte ;
	long		datsiz ;
	int		split ;
	int		first_size ;

	skip_id=0 ;
	firstbyte=rbl_1b();
	if ( firstbyte>=DEF_L1 ) {
		split=1 ;
		switch ( firstbyte ) {
		case DEF_L1 : datsiz=rbl_1b() ; break ;
		case DEF_L2 : datsiz=rbl_2b() ; break ;
		case DEF_L4 : datsiz=rbl_4b() ; break ;
		case DEF_L8 :
		default:      fatal("can not (yet) handle very big data");
			      break ;
		}
		first_size=rbl_2b() ;
	} else {
		split=0 ;
		datsiz=firstbyte ;
	}
	if ( split && datsiz==0 && first_size) {
		warning("inconsistent data description, will try to continue");
		/* Assume the last number (current buffer size) is correct */
		(void)rbl_mem(first_size) ;
	}
	if ( datsiz != 0 ) {
		/* Set everything up for subsequent reads or skips */
		cur_data_id= id ;
		cur_data_p_id= p_id ;
		cur_data_size=datsiz ;
		cur_data_off=0 ;
		cur_data_first_size= ( split ? first_size : datsiz ) ;
	}
	return datsiz ;
}

void
pass_data(length)
	int		length ;
{
	long		after_size ;
	int		use_length ;
	ptr		databuf ;

	after_size= cur_data_off + length ;
	if ( after_size>cur_data_size ) {
		warning("extraneous data for %s, ignoring",id_pn(cur_data_id));
		use_length=cur_data_size-cur_data_off ;
	} else {
		use_length= length ;
	}

	databuf=rbl_mem(length) ;
	data_read=1 ;
	if ( use_length ) {
		(*cur_data_handler)(cur_data_off,use_length,databuf) ;
	}
	cur_data_off += length ;
	if ( cur_data_off>=cur_data_size ) {
		(*cur_data_handler)(0L,0,(ptr)0) ;
		cur_data_id=0 ;
		id_unlock(cur_data_p_id) ;
	}
}

void
start_getdat(handler)
	void		(*handler)();
{
	cur_data_handler=handler ;
	if ( cur_data_first_size ) {
		pass_data(cur_data_first_size) ;
	}
}

static void
skip_data()
{
	if ( cur_data_id ) {
		skip_id=cur_data_id ;
		cur_data_id=0 ;
		id_unlock(cur_data_p_id);
		if ( cur_data_first_size && !data_read) {
			rbl_mem(cur_data_first_size) ;
		}
	} else {
		warning("internal inconsistency, skipping unexpected data");
	}
}

void
get_data(){
	long			id;
	long			off;
	int			buflen ;

	id=rbl_4b();
	if ( id==skip_id ) return ;
	if ( cur_data_id==0 || id!=cur_data_id ) {
		warning("input error, skipped data for id %ld",id);
		if ( cur_data_id ) cur_close() ;
		return ;
	}
	off=rbl_4b();
	if( off!=cur_data_off ) {
		warning("data missing for id %ld, file %s",id,id_pn(id));
		cur_data_off=off ;
		cur_close() ;
		return ;
	}
	buflen=rbl_2b();
	pass_data(buflen);
}

void
get_def() {
	/* Read definition of non-dir capability */
	int			flag;
	long			id;
	register struct cap_t	*p_id ;
	unsigned		cur_server ;
	errstat			err ;
	long			datasize ;
	int			do_skip ;
	capability		my_empty ;

	for(;;) {
		flag=rbl_1b();
		if ( flag==AR_EOD ) return ;
		id=rbl_4b();
		if ( id>max_capid ) max_capid=id ;
		if ( cur_data_id && id!=cur_data_id ) {
			warning("data missing for id %ld, file %s",
				cur_data_id,id_pn(cur_data_id));
			cur_close();
		}
		p_id=id_get(id) ;
		if ( id!=1 && cn_parent(p_id)==0 ) have_unreach=1 ;
		/* Skip dates and version */
		rbl_4b(); rbl_4b(); rbl_4b();
		data_read=0 ; /* flag used by skip_data */
		switch(flag){
		case AR_FILE:
			do_skip=0 ;
			if ( cn_flag(p_id)&(EXISTS|VALID) ) {
				do_skip=1 ;
			} else {
				cn_type(p_id)=C_FILE;
			}
			if ( partial && (cn_flag(p_id)&EXTRACT)==0 ) {
				do_skip=1 ;
			}
			
			cur_server=rbl_2b() ;
			cur_data_size=get_datasize(id,p_id);
			/* Have now read all info */

			if ( do_list && ! do_skip ) {
				TTY_LOCK
				printf("Id %7ld, %s,",id,id_pn(id)) ;
				printf(" file of %ld bytes",cur_data_size);
				if ( verbose>2 ) printf(" on server %d",
					cur_server) ;
				printf("\n");
				TTY_UNLOCK
				do_skip=1 ;
			}
			if ( do_skip ) {
				/* We've scanned the input now */
				/* Will skip the current file */
				if ( cur_data_size>0 ) {
					skip_data() ;
				}
				break ;
			}		

			cn_server(p_id)=cur_server ;
			if ( verbose ) {
				message("reading %s: %ld bytes",id_pn(id),
					cur_data_size) ;
			}
			if ( cur_data_size>0 ) {
				start_getdat(ass_file) ;
			} else {
				err=cre_empty(&my_empty) ;
				if ( err!=STD_OK ) {
warning("could not create empty file %s",id_pn(id));
					bullet_err(err) ;
				} else {
					cs_insert(&cn_capdst(p_id),&my_empty) ;
					cn_flag(p_id) |= (VALID|CREATED) ;
				}
			}
			break ;
		case AR_CAP:
			datasize=get_datasize(id,p_id) ;
			if ( cn_flag(p_id)&(EXISTS|VALID) ) {
				if ( datasize ) skip_data();
				break ;
			}
			if ( cn_type(p_id)==0 ) cn_type(p_id)=C_OTHER;
			if ( !partial || (cn_flag(p_id)&EXTRACT) ) {
				if ( do_list ) {
					TTY_LOCK
					printf("Id %7ld, %s, capability",
							id,id_pn(id)) ;
					if ( datasize!=CAP_SZ ) {
						printf(" (%ld bytes)\n",datasize) ;
					} else {
						printf("\n") ;
					}
					TTY_UNLOCK
					if ( datasize ) {
						if ( verbose<=1 ) {
							skip_data() ;
						} else {
							start_getdat(ass_cap) ;
						}
					}
					break ;
				}
				if ( datasize!=CAP_SZ ) {
					warning("unexpected capability length %d for %s",
						datasize,id_pn(id));
					skip_data();
					break ;
				}
				if ( verbose ) {
					message("reading capability for %s"
						,id_pn(id)) ;
				}
				start_getdat(ass_cap) ;
			} else {
				skip_data();
			}
			break ;
		case AR_NONE:
			datasize=get_datasize(id,p_id) ;
			if ( datasize ) {
				warning("unexpected data for %s",id_pn(id));
				skip_data();
			}
			/* Should have read all data */
			if ( cn_flag(p_id)&(EXISTS|VALID) ) break ;
			cn_type(p_id)=C_NONE;
			cn_flag(p_id) |= VALID ;
			if(!partial||(cn_flag(p_id)&EXTRACT) ){
				if ( do_list ) {
					TTY_LOCK
					printf("Id %7ld, %s, no information present\n",
							id,id_pn(id)) ;
					TTY_UNLOCK
					break ;
				}
				if ( verbose ) {
					message("contents of %s is missing"
						,id_pn(id)) ;
				}
			}
			break ;
		case AR_EMPTY:
			datasize=get_datasize(id,p_id) ;
			if ( datasize ) {
				warning("unexpected data for %s",id_pn(id));
				skip_data();
			}
			/* Should have read all data */
			if ( cn_flag(p_id)&(EXISTS|VALID) ) break ;
			if ( !partial || (cn_flag(p_id)&EXTRACT) ) {
				cn_type(p_id)=C_EMPTY;
				if ( do_list ) {
					TTY_LOCK
					printf("Id %7ld, %s, empty capset\n",
						id,id_pn(id));
					TTY_UNLOCK
					break ;
				}
				if ( verbose ) {
					message("installing empty capability for %s"
						,id_pn(id)) ;
				}
				cn_flag(p_id) |= VALID ;
			}
			break ;
		}
		if ( !cur_data_id) id_unlock(p_id);
	}
}

void
get_contents() {
	/* Get the rest of the tape. Create files for all id's to be
	   extracted.
	   Save the cap in the id table.
	 */
	int		type ;
	int		subtype ;

	for (;;) {
		type=read_block() ;
		switch(type) {
		case -1:
			if ( cur_data_id ) {
				warning("data missing for id %ld, file %s",
					cur_data_id,id_pn(cur_data_id));
				cur_close();
			}
			return ;
		case AR_GRAPH:
			if ( !in_multi ) {
				warning("belated directory definition, skipped");
			}
			continue ;
		case AR_CONTENT:
			break ;
		default:
			warning("unexpected block type, block skipped") ;
			continue ;
		}
		/* Only here when type==AR_CONTENT */
		subtype=rbl_1b() ;
		switch(subtype) {
		case AR_SERVER:
			def_server() ; break ;
		case AR_DEF:
			get_def() ; break ;
		case AR_DATA:
			get_data() ;
			break ;
		default:
			warning("unexpected content subtype, block skipped") ;
			continue ;
		}
	}

}

void
cre_entries()
{
    /* Create all directories */
    register long		id ;
    register struct cap_t	*p_id ;
    register struct dirlist	*p_dl ;
    register struct dir_ent	*p_de ;
    register struct cap_t	*p_id2 ;
    register int		j ;
    errstat			err ;
    capability			cap_restr ;
    capability			cap ;
    capset			cs ;
    int				full ;
    char			*dirname ;
    capset			dircs;

    for ( id=2 ; id<=max_dir_id ; id++ ) {
        p_id=id_get(id) ;
        if ( (!partial || cn_flag(p_id)&EXTRACT) &&
             cn_type(p_id)==C_DIR  ) {
	    dirname=id_pn(id);
            p_dl=cn_dir(p_id) ;
	    if ( !p_dl ) {
		warning("missing information for directory %s",dirname);
		id_unlock(p_id) ;
		continue ;
	    }
            p_de=p_dl->r_entries ;
	    if ( !cs_copy(&dircs,&cn_capdst(p_id)) ) {
		fatal("cs_copy failed");
	    }
            for ( j=0 ; j<p_dl->r_count ; j++, p_de++ ) {
                if ( !(p_de->flags&ENT_VALID) ) continue ;
                if ( partial && !(p_de->flags&ENT_EXTRACT) ) continue ;
                if ( p_de->flags&ENT_EXISTS ) {
			if ( verbose ) {
				message("retaining %s/%s",
					dirname,p_de->name) ;
			}
			continue ;
		}
                p_id2=id_get(acq_cap(p_de));
        	if ( !(!partial || cn_flag(p_id2)&EXTRACT)) {
		    warning("internal inconsistency in cre_entries for %s/%s",
				dirname,p_de->name);
		    continue ;
		}
        	if ( !(cn_flag(p_id2)&VALID) ) {
		    warning("could not extract %s/%s",
				dirname,p_de->name);
		    id_unlock(p_id2) ;
		    continue ;
		}
		cs_to_cap( &cn_capdst(p_id2),&cap);
		full= (p_de->caprights&ALL_RIGHTS)==ALL_RIGHTS ;
		if ( !full &&
		     ( cn_type(p_id2)==C_FILE || cn_type(p_id2)==C_DIR)) {
		    err=std_restrict(&cap,(rights_bits)p_de->caprights,&cap_restr);
		    if ( err!=STD_OK ) {
			warning("restrict failed for %s/%s, installing capability with all rights",
				dirname,p_de->name);
			full=1 ;
		    } else {
			cap= cap_restr ;
		    }
		}
		if ( cn_type(p_id2)==C_EMPTY ) {
		    cs.cs_final=cs.cs_initial=0 ;
		    cs.cs_suite= (suite *)0 ;
		} else {
		    if ( !cs_singleton(&cs,&cap) ) {
		    	fatal("cs_singleton failed");
		    }
		}
                err=sp_append(&dircs,p_de->name,
                    &cs,p_dl->c_count,
                    p_de->rights);
                if ( err==STD_EXISTS ) {
		    if ( retain ) {
warning("directory entry %s in %s suddenly appeared, retaining",
	p_de->name,dirname) ;
			if ( cn_flag(p_id2)&CREATED ) {
			    /* can not install a created directory */
			    p_de->flags &= ~ENT_VALID ;
			    have_unreach=1 ;
			} else {
			    cs_free(&cs);
			    id_unlock(p_id2) ;
			}
			continue ;
		    }
		    if ( !force ) {
			cs_free(&cs);
			fatal("%s/%s exists",dirname,p_de->name);
		    }
                    err=sp_delete(&dircs,p_de->name) ;
		    if ( err==STD_NOTFOUND || err==STD_OK ) {
			if ( err==STD_OK && verbose ) {
				message("removed existing %s/%s",
					dirname,p_de->name) ;
			}
			err=sp_append(&dircs,p_de->name,
				&cs,p_dl->c_count,
				p_de->rights);
			if ( err==STD_EXISTS ) {
				warning("%s/%s still exists after succesfull delete",
					dirname,p_de->name) ;
				cs_free(&cs);
				id_unlock(p_id2) ; continue ;
			}
		    } else {
			warning("could not remove %s/%s",dirname,p_de->name);
			cs_free(&cs);
			id_unlock(p_id2) ; continue ;
		    }
		}
		if ( err!=STD_OK ) {
		    warning("could not install %s/%s \"%s\"",
		    dirname,p_de->name,
		    err_why(err)) ;
		} else {
		    if ( !(cn_flag(p_id2)&EXISTS) ) {
			p_de->flags |= ENT_DONE ;
		    }
		    if ( verbose ) {
			if ( cn_flag(p_id2)&EXISTS ) {

			    message("linked %s/%s",dirname,p_de->name);
			} else {
			    message("extracted %s/%s",dirname,p_de->name);
			}
		    } else {
			if ( cn_flag(p_id2)&EXISTS ) {
			    warning("installed existing capability in %s/%s",
				dirname,p_de->name);
			}
		    }
		}
		cs_free(&cs) ;	
                id_unlock(p_id2) ;
            }
        }
	cs_free(&dircs);
        id_unlock(p_id) ;
    }
}

void
list_dirs()
{
    /* List all entries on the tape  */
    register long		id ;
    register struct cap_t	*p_id ;
    register struct cap_t	*p_id2 ;
    register struct dirlist	*p_dl ;
    register struct dir_ent	*p_de ;
    register int		i ;
    register int		j ;
    char			*dirname ;

    for ( id=1 ; id<=max_dir_id ; id++ ) {
        p_id=id_get(id) ;
        if ( (!partial || cn_flag(p_id)&EXTRACT) &&
             cn_type(p_id)==C_DIR  ) {
	    dirname=id_pn(id);
	    TTY_LOCK
	    if ( verbose!=0 ) {
		if ( id==1 ) {
		    printf("Id 1, Main entry\n") ;
		} else {
		    printf("Id %7ld, %s, directory",id,dirname);
		    if ( verbose>2 ) printf(" on server %d",cn_server(p_id)) ;
		    printf("\n") ;
		}
	    }
	    TTY_UNLOCK
            p_dl=cn_dir(p_id) ;
	    TTY_LOCK
	    if ( !p_dl ) {
		printf(" with missing entry information\n");
		TTY_UNLOCK
		id_unlock(p_id) ;
		continue ;
	    }
	    if ( verbose>1 ) {
		printf("                    [");
		for ( i=0 ; i<p_dl->c_count ; i++ ) {
		    printf("%10s",p_dl->c_names[i]);
		}
		printf("] restrict       Id\n");
	    }
            p_de=p_dl->r_entries ;
            for ( j=0 ; j<p_dl->r_count ; j++, p_de++ ) {
                if ( !(p_de->flags&ENT_VALID) ) continue ;
		TTY_UNLOCK
		p_id2=id_get(acq_cap(p_de));
		TTY_LOCK
        	if ( !(!partial || cn_flag(p_id2)&EXTRACT)) {
		    id_unlock(p_id2) ; 
		    continue ;
		}
		if ( verbose==0 ) {
		    if ( id==1 ) printf("%s\n",p_de->name) ;
		    else printf("%s/%s\n",dirname,p_de->name) ;
		    continue ;
		} else {
		    printf(" %-18s",p_de->name);
		    if ( verbose>1 ) {
			printf("  ");
			for ( i=0 ; i<p_dl->c_count ; i++ ) {
			    printf("  %8x",(int)p_de->rights[i]);
			}
			printf("  %8x ",(int)p_de->caprights);
		    }
		    TTY_UNLOCK
		    printf(" %7ld\n",acq_cap(p_de));
		    TTY_LOCK
		}
		id_unlock(p_id2) ; 
            }
        }
	TTY_UNLOCK
        id_unlock(p_id) ;
    }
}

void
set_reach(id)
	long			id;
{
	register struct cap_t	*p_id ;
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register int		i ;

	p_id=id_get(id) ;
	if ( cn_type(p_id)!=C_DIR ) {
		if ( !(cn_flag(p_id)&M_IGNORE) ) {
			cn_flag(p_id)|= REACHABLE ;
		}
		id_unlock(p_id) ;
		return ;
	}
	if ( cn_flag(p_id)&(LOOPED|M_IGNORE) ) {
		id_unlock(p_id) ;
		return ;
	}
	cn_flag(p_id)|= (LOOPED|REACHABLE) ;
	p_dl=cn_dir(p_id) ;
	if ( !p_dl ) {
		id_unlock(p_id) ;
		return ;
	}
	p_de=p_dl->r_entries ;
	for ( i=0 ; i<p_dl->r_count ; i++, p_de++ ) {
		if ( partial && !(p_de->flags&ENT_EXTRACT) ) continue ;
		if( (p_de->flags&(ENT_EXISTS|ENT_VALID|ENT_DONE))
			==
		    (ENT_VALID|ENT_DONE) ) {
			set_reach(acq_cap(p_de)) ;
		}
	}
	id_unlock(p_id) ;
}

void
check_unreach(id)
	long			id;
{
	register struct cap_t	*p_id ;
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register int		i ;

	p_id=id_get(id) ;
	if ( cn_type(p_id)!=C_DIR ) {
		/* Only dir's interest me now */
		id_unlock(p_id) ;
		return ;
	}
	if ( cn_flag(p_id)&(LOOPED|REACHABLE|M_IGNORE) ) {
		id_unlock(p_id) ;
		return ;
	}
	if ( cn_flag(p_id)&UNR_ENTRY ) {
		/* Ok, we have reached an entry point. This entry
		   point is no longer needed since it can be reached
		   through the current entry point.
		   Reaching the current entry itself is
		   prevented by loop prevention */
		cn_flag(p_id)&= ~UNR_ENTRY ;
	}
	unreach_count++ ;
	cn_flag(p_id)|= (UNR_DIR|LOOPED) ;
	p_dl=cn_dir(p_id) ;
	if ( !p_dl ) {
		id_unlock(p_id) ;
		return ;
	}
	p_de=p_dl->r_entries ;
	for ( i=0 ; i<p_dl->r_count ; i++, p_de++ ) {
		/* Check on partial is superfluous, whole business only
		   is called for full restors's */
		if( (p_de->flags&(ENT_EXISTS|ENT_VALID|ENT_DONE))
			==
		    (ENT_VALID|ENT_DONE) ) {
			check_unreach(acq_cap(p_de)) ;
		}
	}
	id_unlock(p_id) ;
}

void
loop_clear(id)
	long			id;
{
	register struct cap_t	*p_id ;
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register int		i ;

	p_id=id_get(id) ;
	if ( cn_type(p_id)!=C_DIR ) {
		/* Only dir's interest me now */
		id_unlock(p_id) ;
		return ;
	}
	if ( cn_flag(p_id)&REACHABLE || !(cn_flag(p_id)&LOOPED) ) {
		id_unlock(p_id) ;
		return ;
	}
	cn_flag(p_id)&= ~LOOPED ;
	p_dl=cn_dir(p_id) ;
	if ( !p_dl ) {
		id_unlock(p_id) ;
		return ;
	}
	p_de=p_dl->r_entries ;
	for ( i=0 ; i<p_dl->r_count ; i++, p_de++ ) {
		/* Check on partial is superfluous, whole business only
		   is called for full restore's */
		if( (p_de->flags&(ENT_EXISTS|ENT_VALID|ENT_DONE))
			==
		    (ENT_VALID|ENT_DONE) ) {
			loop_clear(acq_cap(p_de)) ;
		}
	}
	id_unlock(p_id) ;
}

void
set_entry_point(id,p_id)
	long			id;
	struct cap_t		*p_id;
{
	/* Now we have an unreachable as an entry point */

	/* Call check_unreach to find everything that was not reachable
	   through id 1 and set everything so reachable to UNR_DIR.
	   Loops are prevented with the LOOPED variable. check_unreach
	   set LOOPED, this routine clears it again.
	 */
	unreach_count=0 ;
	check_unreach(id) ;
	/* Heuristic used to decide whether to clear all LOOPED's or
	   only this reachable through 'id'.
	 */
	if ( unreach_count>(2*max_dir_id/3) ) {
		/* Clear all */
		clear_dir_loops();
	} else {
		loop_clear(id);
	}
	/* Set its entry point flag */
	cn_flag(p_id)|= UNR_ENTRY ;

}

void
#ifdef __STDC__
cre_unreach(void)
#else
cre_unreach()
#endif
{
	register struct cap_t	*p_id ;
	long			id ;
	capset			t_cs;
	errstat			errcode;
	char			*unreach_name;

	/* First clear the LOOPED indicators */
	for ( id=2 ; id<=max_dir_id ; id++ ) {
		p_id=id_get(id) ;
		cn_flag(p_id) &= ~LOOPED ;
		id_unlock(p_id) ;
	}

	/* Then find everything that is reachable */
	set_reach(1L);

	/* Then find the minimal set of entry points to
           every unreachable directory */
	/* Everything reachable through an entry point is set to reachable*/
	
	for ( id=2 ; id<=max_dir_id ; id++ ) {
		p_id=id_get(id) ;
		if ( cn_flag(p_id)&(REACHABLE|EXISTS|UNR_DIR) ) {
			id_unlock(p_id) ;
			continue ;
		}
		if ( !(cn_flag(p_id)&VALID) ) {
			id_unlock(p_id) ;
			continue ;
		}
		/* Set everything reachable through this to UNR_DIR */
		set_entry_point(id,p_id) ;
		id_unlock(p_id) ;
	}
	/* Chech the existance of the directory used to store the
	   unreachables. Create it if non-existant.
	 */
	errcode=sp_lookup(SP_DEFAULT,unreach_path,&unreach_dir_cs) ;
	if ( errcode!=STD_OK ) {
		if ( errcode==STD_NOTFOUND ) {
			unreach_name= unreach_path ;
			errcode=sp_traverse(SP_DEFAULT,
					  (const char **)&unreach_name,&t_cs) ;
			if ( errcode!=STD_OK ) {
				fatal("can not access \"%s\"(%s)",
					unreach_path,err_why(errcode));
			}
			errcode=sp_create(dir_cs,sts_columns,&unreach_dir_cs) ;
			if ( errcode!=STD_OK ) {
				fatal("can not create \"%s\"(%s)",
					unreach_path,err_why(errcode));
			}
			errcode=sp_append(&t_cs,unreach_name,&unreach_dir_cs,
				sts_ncols,sts_rights) ;
			cs_free(&t_cs);
			if ( errcode!=STD_OK ) {
				fatal("can not create \"%s\"(%s)",
					unreach_path,err_why(errcode));
			}
		} else {
			fatal("can not access \"%s\"(%s)",
				unreach_path,err_why(errcode));
		}
	}

	/* Now first install all entry point dirs, setting everything
	   reachable through
	   them to reachable, and then gather all non-dir capabilities
	 */
	for ( id=2 ; id<=max_dir_id ; id++ ) {
		p_id=id_get(id) ;
		if ( (cn_flag(p_id)&(UNR_ENTRY|VALID))!=(UNR_ENTRY|VALID) ) {
			id_unlock(p_id) ;
			continue ;
		}
		/* Now we an entry point */
		install_unreach(id,p_id) ;
		/* Note all reachable as such */
		set_reach(id) ;
		id_unlock(p_id) ;
	}
	/* Lastly go through all id's to install non dir id's */
	for ( id=2 ; id<=max_capid ; id++ ) {
		p_id=id_get(id) ;
		if ( (cn_flag(p_id)&(REACHABLE|VALID))!=VALID ) {
			id_unlock(p_id) ;
			continue ;
		}
		/* Now we an entry point */
		install_unreach(id,p_id) ;
		/* Setting things to reachable is useless, we won't look
		  at it any more. */
		id_unlock(p_id) ;
	}
	/* Sigh, all done */
}

static void
#ifdef __STDC__
install_unreach(long id,struct cap_t *p_id)
#else
install_unreach(id,p_id)
	long			id;
	struct cap_t		*p_id;
#endif
{
	char		id_name[10] ;
	capset		id_cs ;
	errstat		errcode ;
	int		result ;

	sprintf(id_name,"%06ld",id) ;
	result=cs_copy(&id_cs,&cn_capdst(p_id));
	if ( !result ) fatal("cs_copy failed in install_unreach");
	errcode=sp_append(&unreach_dir_cs,id_name,&id_cs,sts_ncols,sts_rights);
	if ( errcode!=STD_OK ) {
		if ( errcode==STD_EXISTS ) {
			if ( retain ) {
                        	if ( verbose ) message(
                            		"retaining %s/%s",unreach_path,id_name) ;
			} else if ( force ) {
				errcode=sp_delete(&unreach_dir_cs,id_name) ;
                        	if ( errcode==STD_OK && verbose )  {
					message("removed existing %s/%s",
						unreach_path,id_name) ;
				}
				errcode=sp_append(&unreach_dir_cs,id_name,&id_cs,sts_ncols,sts_rights);
				if ( errcode!=STD_OK ) {
					warning("could not create \"%s/%s\"(%s)",
						unreach_path,id_name,
						err_why(errcode));
				} else {
					if ( verbose ) {
						message("installed %s/%s",
							unreach_path,id_name);
					}
				}
			} else {
				warning("could not create \"%s/%s\"(%s)",
						unreach_path,id_name,
						err_why(errcode));
			}
		} else {
			warning("could not create \"%s/%s\"(%s)",
						unreach_path,id_name,
						err_why(errcode));
		}	
	} else {
		if ( verbose ) {
			message("installed %s/%s",
						unreach_path,id_name);
		}
	}
	cs_free(&id_cs);
}

void
del_entries(id,p_id)
	long		id ;
	struct cap_t	*p_id ;
{
	register struct dirlist	*p_dl ;
	register struct dir_ent	*p_de ;
	register int		j ;
	capset			dircs ;

        p_dl=cn_dir(p_id) ;
	if ( !p_dl ) {
		id_unlock(p_id) ;
		return ;
	}
        p_de=p_dl->r_entries ;
	if ( !cs_copy(&dircs,&cn_capdst(p_id)) ) {
		fatal("cs_copy failed");
	}
        for ( j=0 ; j<p_dl->r_count ; j++, p_de++ ) {
                if ( !(p_de->flags&ENT_VALID) ) continue ;
                if ( partial && !(p_de->flags&ENT_EXTRACT) ) continue ;
                if ( p_de->flags&ENT_EXISTS ) continue ;
		sp_delete(&dircs,p_de->name);
	}
	cs_free(&dircs);
}

void
#ifdef __STDC__
del_unreach(void)
#else
del_unreach()
#endif
{
	register struct cap_t	*p_id ;
	long			id ;
	long			failcnt ;
	static			working ; /* Used to prevent recursion */

	if ( working ) return ; working=1 ;
	/* The offensive file could be the one we are busy reading */
	if ( cur_file_stat==1 ) {
		std_destroy(&cur_file_cap);
	}

	/* First clear the LOOPED indicators */
	clear_dir_loops() ;
	/* Then find everything that is reachable */
	set_reach(1L);

	/* Then destroy everything unreachable */
	failcnt=0 ;
	for ( id=2 ; id<=max_capid ; id++ ) {
		p_id=id_get(id) ;
		if ( (cn_flag(p_id)&(REACHABLE|EXISTS)) ||
		      !(cn_flag(p_id)&CREATED) ) {
			id_unlock(p_id);
			continue ;
		}
		if ( cn_type(p_id)==C_DIR ) del_entries(id,p_id) ;
		if ( cs_purge(&cn_capdst(p_id))!=STD_OK ) failcnt++ ;
		id_unlock(p_id) ;
	}
	if ( failcnt ) warning("failed to destroy %ld capabilities at abort",
					failcnt);
	working=0 ;
}

static void
clear_dir_loops()
{
	register long id ;
	struct cap_t *p_id ;

	for ( id=2 ; id<=max_dir_id ; id++ ) {
		p_id=id_get(id) ;
		cn_flag(p_id) &= ~LOOPED ;
		id_unlock(p_id) ;
	}
}

void
#ifdef __STDC__
killed(int sig)
#else
killed(sig)
	int	sig;
#endif
{
	warning("emergency stop due to signal, destroying unused capabilities");
	del_unreach();
	exit(-1) ;
}

static int
better_rights(cap,cs)
	capability	*cap;
	capset		*cs;
{
	register int	i ;
	long		rights ;
	long		caprights ;
	int		better ;

	better=0 ;
	caprights= cap->cap_priv.prv_rights ;

	for ( i = 0; i<cs->cs_final; i++ ) {
		if ( cs->cs_suite[i].s_current ) {
			rights = cs->cs_suite[i].s_object.cap_priv.prv_rights ;
			if ( rights!=caprights &&
			     (rights&caprights)==caprights ) {
				/* Only real improvements count */
				*cap=cs->cs_suite[i].s_object ;
				caprights=rights ;
				better=1 ;
			}
		}
	}
	return better ;
}

errstat
#ifdef __STDC__
cre_empty(capability *cap)
#else
cre_empty(cap)
	capability		*cap ;
#endif
{
	errstat		err ;

	if ( !ok_bullet ) init_bullet() ;
	err=b_create(&bullet_server,(char *)0,(b_fsize)0,BS_COMMIT,cap) ;
	return err ;
}

#ifdef MEMDEBUG
get_cleanup() {
	extern	ptr		cur_unit ;
	extern	char 		*r_format;
	extern	char 		*r_version;
	extern	char 		*r_sdate;

	/* cleanup all mem allocated by getmem */
	id_cleanup() ;
	if ( !do_stdio) cs_free(&arch_cs) ;
	if ( dir_cs ) cs_free(dir_cs) ;
	if ( cur_unit ) free((char *)cur_unit) ;
	if ( r_format ) free(r_format) ;
	if ( r_version ) free(r_version) ;
	if ( r_sdate ) free(r_sdate) ;
}
#endif
