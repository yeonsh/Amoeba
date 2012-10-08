/*	@(#)put.c	1.8	96/02/27 13:16:19 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* STARCH - STore and reSTore ARCHives of graphs.
   module implementing -o option

	1 - Use dgwalk to build up an image of the graph in core
	2 - Traverse the graph to put graph information in the archive
	3 - Traverse the graph looking for descriptions to put in the archive

*/

#include "starch.h"
#include "block.h"
#include <sp_dir.h>
#include <module/dgwalk.h>
#include <module/goodport.h>

static long	max_dir_id ;	/* highest internal cap # of dirs to be dumped */
static char	*ar_path ;	/* Set by w_open, used by w_close */

static int	domg();
static long	worst_rights();
static void	revamp();
static void	revisit();

#ifdef __STDC__
void	w_open(char *name,char *desc);
void	w_close();
void	aw_df(void);
void	aw_empty(struct cap_t *p_id, long id);
void	get_entries(void);
void	write_tape(long);
void	write_serv(void);
void	write_id(long);
void	aw_fwr(long id,byte *buf,long offset,long amount,int dowrite);
#else
void	w_open();
void	w_close();
void	aw_df();
void	aw_empty() ;
void	get_entries();
void	write_tape();
void	write_serv();
void	write_id();
void	aw_fwr();
#define const /**/
#endif

int		def_ready ;	/* Do we have an AR_DEF open ? */
static byte	*filebuf ;	/* The major output buffer */
int		description_dump ; /* Second phase archive writing */

int
doput()
{
	init_hash();
	init_id();
	/* First create image of graph */

	if ( !dowalk(domg) ) return 1 ;

	w_open(archive,"output archive");
	/* get the full graph info */
	max_dir_id=max_capid ;
	get_entries();
	hash_cleanup();

	write_tape(0L) ;

	/* We can do this here because ar_flush flushed the write thread */
	w_close();

	if ( ddo_list.first ) {
		/* Make the dump description */
		description_dump=1 ;
		message("Writing description dump");
		err_flush();
		f_next(&ddo_list);
		w_open(archive,"\"-a\" archive") ;
		write_tape(0L);
		w_close();
	}

	if ( stats ) iostats() ;

#ifdef MEMDEBUG
	put_cleanup();
#endif
	return 1 ;
}

int
dorescan()
{
	errstat		errcode;
	long		first_dumpcap ;

	init_id();

	/* Some initial preparations */
	if ( files ) fatal("paths arguments indicating starting points can not be used with -A");

	/* Try to access the archive */
	/* Dirty tricks department, reset arch_list, because
	   set_archive's result are used later on.
	 */
	arch_list.next= arch_list.first ;
	/* Now access -A as if nothing happened */
	f_next(&ddi_list) ;
	openarch(archive,0,0,0) ;
	ar_type="\"-A\" archive" ;
	if ( !do_stdio ) get_disksize(1) ;
	init_read();

	/* Assemble the graph info */
	get_graph();

	get_contents() ;
	
	if ( io_type==IO_TAPE ) {
		/* The reader thread must be finished by now */
		errcode=tape_rewind(&arch_cap,0) ;
		if (errcode!=STD_OK ) {
			warning("could not rewind tape") ;
		}
	}

	/* Create and fill hash table */
	init_hash();
	id_hash();

	first_dumpcap= max_capid ;

	/* Open the output archive ... */
	f_next(&arch_list) ;
	w_open(archive,"output archive");

	/* get the full graph info */
	max_dir_id=max_capid ;

	/* Re-visit */
	revisit() ;

	/* Scan new directories */
	revamp();

	hash_cleanup();

	/* Signal write_tape to go through ALL id's for dir's */
	max_dir_id= max_capid ;
	write_tape(first_dumpcap) ;

	/* We can do this here because ar_flush flushed the write thread */
	w_close();

	if ( ddo_list.first ) {
		/* Make the dump description */
		description_dump=1 ;
		message("writing description dump");
		f_next(&ddo_list);
		w_open(archive,"\"-a\" archive") ;
		write_tape(0L);
		w_close();
	}

	if ( stats ) iostats() ;

#ifdef MEMDEBUG
	put_cleanup();
	/* Put cleanup left 0's at the relevant places. get_cleanup
	   checks for these zeros to avoid freeing memory twice.
	 */
	get_cleanup() ;
#endif
	return 1 ;
}

static long
get_path_id(paths)
	dgw_paths	*paths;
{
	long		id;
	long		pid;	/* Parent id */	
	int		new;
	struct cap_t	*p_id;

	/* Get a directory's pid */
	/* and get the pid of the parent too... */
	id=dircap(&paths->cap,&new,&p_id);
	if ( !new ) {
		if ( cn_flag(p_id)&LOOPED ) {
			/* We got into a loop, try to get out */
			if ( paths->dotdot ) {
				/* Try again */
				cn_parent(p_id)=get_path_id(paths->dotdot) ;
			} else {
				cn_parent(p_id)=1 ;
			}
		}
		return id ;
	}
	cn_flag(p_id) |= LOOPED ; /* Set loop indicator */
	cn_parent(p_id)=0 ;
	cn_type(p_id)=C_DIR;
	if ( paths->dotdot ) {
		pid=get_path_id(paths->dotdot);
		if ( cn_parent(p_id)==0 ) {
			/* Non-zero means set by higher powers,
			   beleive in them */
			cn_parent(p_id)=pid ;
		}
	} else {
		cn_parent(p_id)=1 ;
	}
	cn_flag(p_id) &= ~LOOPED ; /* Clear loop indicator */	
	id_unlock(p_id) ;
	return id ;
}
	
static int
domg(paths)
	dgw_paths	*paths;
{
	get_path_id(paths) ;
	return 1 ;
}

/* For each directory, put it in the hash table (if not already there)
   and flag that it is newer.
 */
static int
redomg(paths)
	dgw_paths	*paths;
{
	long		id ;
	struct cap_t	*p_id ;

	id=get_path_id(paths) ;

	p_id=id_get(id) ;
	cn_flag(p_id) |= NEWER ;
	id_unlock(p_id) ;
	
	return 1 ;
}

static void
revisit() {
	long			id;
	struct cap_t		*p_id ;
	capset			**servers ;
	struct pathlist		newdir ;
	sp_seqno_t		latest_version ;
	errstat			err ;
	int			scan ;
	int			max_scan_id ;

	/* For each directory that has changed, rescan i and its new children
	 */
	servers= srch_servers() ;
	newdir.next= (struct pathlist *)0 ;
	max_scan_id= max_capid ; /* max_capid can increase during the scan ..*/
	for ( id=2 ; id<=max_scan_id ; id++ ) {
		p_id= id_get(id) ;
		if ( cn_type(p_id)!=C_DIR ) continue ;
		scan= 0 ;
		if ( !cn_dir(p_id) ) {
			/* No information on tape, is there info in soap now? */
			scan=1 ;
		} else {
			err=sp_getseqno(&cn_capsrc(p_id),&latest_version);
			switch( err ) {
			case STD_OK:
				/* The version is a 64-bit number. We check only
				   for equality. And the implementor told us
				   that soap will never return zero, so we can
				   use 0 as a default value.
				 */
				if ( latest_version.seq[0]!=
					cn_dir(p_id)->d_version.seq[0]
				     ||
				     latest_version.seq[1]!=
					cn_dir(p_id)->d_version.seq[1] ) {
					scan=1 ;
				}
				break ;
			case SP_UNAVAIL:
			case STD_CAPBAD:
				break ;
			default:
				warning("could not get seqno for directory \"%s\", %s",
					id_pn(id),err_why(err));
			case STD_COMBAD:
				scan=1 ;
				break ;
			}
		}
		if ( scan ) {
			newdir.caps= cn_capsrc(p_id) ;
			newdir.path= keepstr(id_pn(id)) ;
			duck(redomg,servers,&newdir) ;
			free(newdir.path) ;
		}
		id_unlock(p_id);
	}
	fr_servers(servers);
}

static void
get_rows(p_id,id,dd)
	register struct cap_t	*p_id ;
	long			id ;
	SP_DIR			*dd ;
{
	register int		i ;
	sp_entry		*slu_i ;
	sp_result		*slu_o ;
	errstat			errcode ;
	register int		res ;
	int			nrows ;
	long			ent_id ;

	cn_dir(p_id)->r_entries=
		(struct dir_ent *)getmem((unsigned)
			(dd->dd_nrows*sizeof (struct dir_ent))) ;
	slu_i=(sp_entry *)getmem((unsigned)
			(dd->dd_nrows*sizeof (sp_entry)));
	slu_o=(sp_result *)getmem((unsigned)
			(dd->dd_nrows*sizeof (sp_result)));
	for ( i=0 ; i<dd->dd_nrows ; i++ ) {
		slu_i[i].se_capset= dd->dd_capset;
		slu_i[i].se_name= dd->dd_rows[i].d_name ;
	}
	errcode=sp_setlookup(dd->dd_nrows,slu_i,slu_o);
	nrows=0 ;
	for ( i=0 ; i<dd->dd_nrows ; i++ ) {
		if ( slu_o[i].sr_status!=STD_OK ) {
			warning("could not access %s/%s",
				id_pn(id),
				dd->dd_rows[i].d_name);
			continue ;
		}
		res=testfile(&slu_o[i].sr_capset,dd->dd_rows[i].d_name) ;
		if ( res==M_IGNORE ) {
			cs_free(&slu_o[i].sr_capset) ;
			continue ;
		}
		ent_id=findcs(&slu_o[i].sr_capset,res,id) ;
		cn_dir(p_id)->r_entries[nrows].name=
			keepstr(dd->dd_rows[i].d_name) ;
		cn_dir(p_id)->r_entries[nrows].rights=
			(long *)keepmem((char *)dd->dd_rows[i].d_columns,
				dd->dd_ncols*sizeof(long)) ;
		cn_dir(p_id)->r_entries[nrows].time=slu_o[i].sr_time ;
		cn_dir(p_id)->r_entries[nrows].flags=ENT_VALID ;
		cn_dir(p_id)->r_entries[nrows].capno=ent_id ;
		cn_dir(p_id)->r_entries[nrows].caprights=
			worst_rights(&slu_o[i].sr_capset) ;
		cs_free(&slu_o[i].sr_capset) ;
		nrows++ ;
	}
	if ( nrows!=dd->dd_nrows ) {
		cn_dir(p_id)->r_entries=
			(struct dir_ent *)adjmem(
				cn_dir(p_id)->r_entries,
				(unsigned)(nrows*sizeof (struct dir_ent))) ;
	}
	cn_dir(p_id)->r_count=nrows ;
	free((char *)slu_i) ; free((char *)slu_o) ;
}

void
get_dir(id,p_id)
	long		id;
	struct cap_t	*p_id ;
{
	SP_DIR			*dd ;
	int			i;
	errstat			errcode;
	sp_seqno_t		latest_version ;

	if ( cn_type(p_id)!=C_DIR ) return ;

	/* If ddo_list is specified we will dump the version number
	 * on the status file.
	 * Because the contents can be modified bewteen the fetching of
	 * the contents and the fetching of the version number we need to
	 * get the version number first.
	 */
	latest_version.seq[0]= 0 ;
	latest_version.seq[1]= 0 ;
	if ( ddo_list.first ) {
		errcode=sp_getseqno(&cn_capsrc(p_id),&latest_version);
		switch ( errcode ) {
		case STD_OK:
			break ;
		case STD_COMBAD:
			{ static int didwarn ;
			  if ( !didwarn ) {
				warning("could not get soap seqno for some directories, using non-existing seqno");
				didwarn=1 ;
			  }
			}
			break ;
		default:
			/* We can not use id_pn here, because the directory
			 * data for the full graph is not yet complete.
			 */
			warning("could not get soap seqno, error %s",
				err_why(errcode));
		}
	}
		
	errcode=sp_list(&cn_capsrc(p_id),&dd) ;
	if ( errcode!=STD_OK ) {
		warning("could not access directory %s",id_pn(id));
		cn_flag(p_id) |= ERROR ;
	} else {
		cn_dir(p_id)=MYMEM(struct dirlist) ;
		cn_dir(p_id)->d_version= latest_version ;
		cn_dir(p_id)->c_count=dd->dd_ncols ;
		if ( dd->dd_ncols ) {
			cn_dir(p_id)->c_names= (char **)
			    getmem((unsigned)(dd->dd_ncols*sizeof (char *))) ;
		} else {
			cn_dir(p_id)->c_names= (char **)0 ;
		}
		for ( i=0 ; i<dd->dd_ncols ; i++ ) {
			cn_dir(p_id)->c_names[i]=keepstr(dd->dd_colnames[i]) ;
		}
		if ( dd->dd_nrows ) get_rows(p_id,id,dd) ;
		sp_closedir(dd);		
	}
}

void
get_entries()
{
	/* Write out the definition of all directories,
	   assigning internal cap #'s to entries not seen
	*/
	long			id ;
	register struct cap_t	*p_id ;
	register int		i,j ;
	errstat			errcode ;
	register int		res ;
	register struct pathlist *flist ;
	sp_entry		sli1 ;
	sp_result		slo1 ;
	char			*path_ptr ;
	long			*toprights ;
	int			spec_entry ;

	/* First create the info in entry 1 by hand */
	p_id=id_get(1L) ;
	cn_type(p_id)=C_DIR ;
	cn_flag(p_id)=M_CONTENT ;
	cn_dir(p_id)=MYMEM(struct dirlist) ;
	cn_dir(p_id)->c_count=TOP_NCOLS ;
	cn_dir(p_id)->c_names=(char **)getmem(TOP_NCOLS*sizeof(char *)) ;
	for ( i=0 ; i<TOP_NCOLS ; i++ ) {
		cn_dir(p_id)->c_names[i]=keepstr("");
	}
	for ( i=0, flist=files ; flist ; flist=flist->next ) i++ ;
	cn_dir(p_id)->r_count=i ;
	if ( i==0 ) {
		warning("writing empty archive") ;
		cn_dir(p_id)->r_entries= (struct dir_ent *)0 ;
	} else {
		cn_dir(p_id)->r_entries=
			(struct dir_ent *)getmem((unsigned)
					(i*sizeof (struct dir_ent))) ;
	}
	for ( i=0, flist=files ; flist ; i++, flist=flist->next ) {
                /* First `find' the top level paths */
		/* dir's have already been entered by domg, they should
		   be found. Non-dirs use the second & third parameter */
		res=testfile(&flist->caps,flist->path) ;
		if ( res&M_IGNORE ) {
			warning("ignoring %s",flist->path) ;
		}
		id=findcs(&flist->caps,res,1L) ;
		if ( id<=0 )
			fatal("internal: lost track of %s",flist->path) ;
		cn_dir(p_id)->r_entries[i].capno=id ;
		cn_dir(p_id)->r_entries[i].flags=ENT_VALID ;
		cn_dir(p_id)->r_entries[i].name=keepstr(flist->path) ;
		cn_dir(p_id)->r_entries[i].caprights=worst_rights(&flist->caps) ;
		path_ptr= flist->path ;
		spec_entry=0 ;
		if ( *path_ptr==0 ||
		     strcmp(".",path_ptr)==0 ||
		     strcmp("..",path_ptr)==0 ) {
			spec_entry= 1 ;
		}
		if ( spec_entry ) {
			cn_dir(p_id)->r_entries[i].time=0 ;
		} else {
			errcode=sp_traverse(SP_DEFAULT,(const char **)&path_ptr,
					    &sli1.se_capset) ;
			if ( errcode!=STD_OK ) {
				warning("can not get data on %s",flist->path) ;
				cn_dir(p_id)->r_entries[i].time=0 ;
			} else {
				sli1.se_name=path_ptr ;
				sp_setlookup(1,&sli1,&slo1);
				if ( slo1.sr_status==STD_OK ) {
					cn_dir(p_id)->r_entries[i].time=slo1.sr_time;
					cs_free(&slo1.sr_capset);
					} else {
					warning("can not get data on %s",flist->path) ;
					cn_dir(p_id)->r_entries[i].time=0 ;
				}
			}	
		}
		toprights=(long *)getmem(TOP_NCOLS*sizeof(long)) ;
		for ( j=0 ; j<TOP_NCOLS ; j++ ) {
			toprights[j]=0;
		}
		if ( !spec_entry ){
		    /* do not do this for . and .. */
		    for ( j=TOP_NCOLS ; j>0 ; j-- ) {
			/* getmasks gives ARGS_BAD for incorrect count */
			/* So we simply try a few times, if none works out
			   we simply assume that all is 0 or the failed
			   getmasks did put something there. (As was the
			   case when this program was written )
			 */
			errcode=sp_getmasks(&sli1.se_capset,sli1.se_name,j,toprights);
			if ( errcode==STD_OK ) break ;
		    }
		    if ( errcode!=STD_OK ) {
			warning("can not get masks for %s",flist->path) ;
		    }
		    cs_free(&sli1.se_capset) ;
		}
		cn_dir(p_id)->r_entries[i].rights=toprights ;
	}

	/* OK, done that */
	id_unlock(p_id) ;
	/* Now assemble and remember info for all other dirs */
	for ( id=2 ; id<=max_dir_id ; id++ ) {
		p_id=id_get(id) ;
		get_dir(id,p_id) ;
		id_unlock(p_id) ;
	}
}

static void
revamp() {
	long			id;
	long			max_test_id;
	struct cap_t		*p_id ;

	max_test_id= max_capid ; /* Max capid will change ... */
	for ( id=2 ; id<=max_test_id ; id++ ) {
		p_id=id_get(id) ;
		if ( cn_type(p_id)==C_DIR && cn_flag(p_id)&NEWER ) {
			dir_free(p_id);
			get_dir(id,p_id) ;
		}
		id_unlock(p_id);
	}
}

void
write_tape(firstcap)
	long		firstcap ;
{
	errstat			errcode ;

	if ( io_type==IO_FILE && !do_stdio ) {
		/* Create the archive, current cap indicates server */
		errcode=b_create(&arch_cap, (char *)0, (b_fsize)0, 0, &arch_cap) ;
		if ( errcode!=STD_OK ) {
			fatal("can not create archive \"%s\"",archive) ;
		}
		arch_valid=1 ;
	}
	init_write();
	/* Write out the first header */
	aw_hdr(AR_START) ;
	/* Write out the server definitions */
	write_serv() ;
	/* Write out the id's */
	/* Directories are first, this allows partial restore */
	write_id(firstcap) ;
	/* Signal end-of-archive */
	aw_hdr(AR_END) ;
	ar_flush() ;
}

void
write_serv(){
	register struct serverlist	*server ;
	int				o_count ;
	int				tmpcount ;
	int				count ;

	w_start() ;
	wbl_1b(AR_CONTENT);
	wbl_1b(AR_SERVER);
	o_count=wbl_tell();
	wbl_2b(0);
	count=0 ;
	for ( server=arch_slist ; server ; server=server->next ) {
		if ( wbl_tell()+CAP_SZ+2+1>=BLOCK_SIZE ) {
			tmpcount=wbl_tell();
			wbl_seek(o_count) ;
			wbl_2b((unsigned)count) ;
			wbl_seek(tmpcount) ;
			w_end();
			w_start() ;
			wbl_1b(AR_CONTENT);
			wbl_1b(AR_SERVER);
			o_count=wbl_tell();
			wbl_2b(0);
			count=0 ;
		}
		wbl_2b((unsigned)server->number) ;
		wbl_cap(&server->server) ;
		wbl_1b(server->i_type) ;
		count++ ;
	}
	tmpcount=wbl_tell();
	wbl_seek(o_count) ;
	wbl_2b((unsigned)count) ;
	wbl_seek(tmpcount) ;
	w_end();
}

void
write_id(firstcap)
	long		firstcap;
{
	long			id ;
	long			f_fid ; /* First file id */
	register struct cap_t	*p_id ;

	f_fid=0 ;
	aw_gs() ; /* Signal start of graph definition */
	for ( id=1 ; id<=max_dir_id ; id++ ) {
		p_id=lid_get(id) ;
		if ( cn_flag(p_id)&LINKED ) {
			id_unlock(p_id) ;
			continue ;
		}
		if ( cn_type(p_id)!=C_DIR ) {
			if ( !f_fid ) f_fid=id ;
			id_unlock(p_id);
			continue ;
		}
		if ( (cn_flag(p_id)&ERROR) || !cn_dir(p_id) ) {
			if ( !cn_dir(p_id) ) {
				warning("internal, missing information for %s",
					id_pn(id));
			}
			cn_type(p_id)= C_NONE ;
			if ( !f_fid ) f_fid=id ;
			id_unlock(p_id);
			continue ;
		}
		aw_dir(p_id,id) ;
		id_unlock(p_id);
	}
	/* Now write it to the archive */
	aw_gf() ;/* Signal end of graph definition */
	if ( description_dump ) {
		/* For a description dump we want all cap's on the output */
		f_fid=2 ;
	} else {
		if ( !f_fid ) f_fid=max_dir_id+1 ;
	}
	if ( firstcap && f_fid<firstcap ) f_fid=firstcap ;
	for ( id=f_fid ; id<=max_capid ; id++ ) {
		p_id=lid_get(id) ;
		if ( cn_flag(p_id)&ERROR ) cn_type(p_id)= C_NONE ;
		if ( (!description_dump && cn_type(p_id)==C_DIR) ||
		     (cn_flag(p_id)&LINKED))
			/* Do nothing */;
		else if ( cn_type(p_id)==C_NONE ) aw_none(p_id,id) ;
		else if ( cn_flag(p_id)&M_CAP || description_dump ) {
			aw_cap(p_id,id) ;
		}
		else if ( cn_flag(p_id)&M_CONTENT ) aw_file(p_id,id) ;
		id_unlock(p_id);
	}
	aw_df() ; /* Signal end of data */
}

#ifdef __STDC__
void	aw_gs(void)
#else
void	aw_gs()
#endif
{}

#ifdef __STDC__
void	aw_dir(struct cap_t *p_id, long id)
#else
void	aw_dir(p_id,id)
	struct cap_t	*p_id;
	long		id;
#endif
{
	int		tmp_off ;
	int		cnt_off ;
	int		ret_off ;
	char		*dummy = "a" ;
	register int	i;
	int		entry_no ;
	int		entry_sz ;
	unsigned	entries_in_block ;
	register struct dir_ent *p_entry ;
	char		*tmpkeep ;
	int		nofit ;

	w_start() ;
	wbl_1b(AR_GRAPH);
	wbl_1b(AR_DIRDEF);
	if ( verbose && id!=1) {
		message("archiving directory %s",id_pn(id));
	}
	wbl_4b(id);
	wbl_2b((unsigned)cn_server(p_id));
	wbl_4b(0L); /* No date known here */
	wbl_4b(cn_dir(p_id)->d_version.seq[0]); /* Directory version number, was 2nd date */
	wbl_4b(cn_dir(p_id)->d_version.seq[1]); /* Directory version number, was 3rd date */
	if ( cn_dir(p_id)->c_count>26 ) {
		cn_dir(p_id)->c_count=26;
		warning("only dumping the first %d colums of %s",id_pn(id));
	}
	wbl_2b(cn_dir(p_id)->c_count) ;
	tmp_off=wbl_tell() ; nofit=0 ;
	for ( i=0 ; i<cn_dir(p_id)->c_count ; i++ ) {
		if (wbl_tell()+strlen(cn_dir(p_id)->c_names[i])+1>=BLOCK_SIZE)
		{	nofit=1 ; break ; }
		wbl_string(cn_dir(p_id)->c_names[i]) ;
	}
	if ( nofit ) {
		wbl_seek(tmp_off) ;
		warning("column names did not fit for %s, replaced by dummies",id_pn(id)) ;
		for ( i=0 ; i<cn_dir(p_id)->c_count ; i++ ) {
			if (wbl_tell()+strlen(dummy)+1>=BLOCK_SIZE) {
				fatal("even dummies did not fit") ;
			}
			wbl_string(dummy) ;
		}
	}
	wbl_4b((long)cn_dir(p_id)->r_count);
	cnt_off=wbl_tell() ;
	wbl_2b(0) ; /* to be filled in */
	entries_in_block=0 ;
	for ( entry_no=0 ; entry_no<cn_dir(p_id)->r_count ; entry_no ++ ) {
		p_entry= &cn_dir(p_id)->r_entries[entry_no];
		if ( !(p_entry->flags&ENT_VALID) ) continue ; /* Ignore faulty */
		p_entry->capno= id_link(acq_cap(p_entry)) ;
		if ( !equiv ) {
			fatal("-e is not implemented") ;
		} else {
			entry_sz= 2 + 4 + 4 ;
		}
		entry_sz += 3*4+cn_dir(p_id)->c_count*4+
			    strlen(p_entry->name)+1;
		if ( wbl_tell()+entry_sz >BLOCK_SIZE ) {
			ret_off=wbl_tell() ;
			wbl_seek(cnt_off) ;
			wbl_2b(entries_in_block) ;
			wbl_seek(ret_off) ;
			w_end() ;
			entries_in_block=0 ;
			w_start() ;
			wbl_1b(AR_GRAPH);
			wbl_1b(AR_DIRCONT);
			wbl_4b(id);
			cnt_off=wbl_tell() ;
			wbl_2b(0) ; /* to be filled in */
		}
		if ( wbl_tell()+entry_sz >BLOCK_SIZE ) {
			if ( BLOCK_SIZE-wbl_tell() <200 ) {
				fatal("internal: block size too small") ;
			}
			entry_sz= BLOCK_SIZE-((5+cn_dir(p_id)->c_count)*4+2);
			tmpkeep=keepstr(p_entry->name) ;
			p_entry->name[entry_sz]=0 ;
			warning("entry name \"%s\" in %s truncated to \"%s\"",
				tmpkeep,id_pn(id),p_entry->name) ;
			free((char *)tmpkeep) ;
		}
		/* Now we know we have enough free room,
		   start writing
		*/
		wbl_string(p_entry->name) ;
		wbl_4b(p_entry->time) ;
		wbl_4b(0L); wbl_4b(0L); /* Two dummy dates */
		for ( i=0 ; i<cn_dir(p_id)->c_count ; i++ ) {
			wbl_4b(p_entry->rights[i]) ;
		}
		wbl_4b(acq_cap(p_entry));
		wbl_4b(p_entry->caprights);
		entries_in_block++ ;
	}
	ret_off=wbl_tell() ;
	wbl_seek(cnt_off) ;
	wbl_2b(entries_in_block) ;
	wbl_seek(ret_off) ;
	w_end() ;
}


#ifdef __STDC__
void	aw_gf(void)
#else
void	aw_gf()
#endif
{
	w_start() ;
	wbl_1b(AR_GRAPH);
	wbl_1b(AR_DIREND);
	w_end() ;
	/* Open a block for capability definitions */
	def_ready=0 ;
}

#ifdef __STDC__
void	aw_file(struct cap_t *p_id, long id)
#else
void	aw_file(p_id,id)
	struct cap_t	*p_id;
	long		id;
#endif
{
	errstat		errcode;
	int		retrack ;
	b_fsize		didread ;
	long		totread ;
	long		datasize ;
	int		stillfree ;


	if ( def_ready && wbl_tell()+1+4+3*4+2+5+2+5>BLOCK_SIZE ) {
		/* This implementation checks whether a zero-length file
		   fits */
		wbl_1b(AR_EOD) ; w_end() ;
		def_ready=0 ;
	}
	if ( !def_ready ) {
		w_start() ; wbl_1b(AR_CONTENT) ; wbl_1b(AR_DEF);
		def_ready=1 ;
	}
	retrack=wbl_tell();
	wbl_1b(AR_FILE);
	if ( verbose ) message("archiving contents of %s",id_pn(id));
	wbl_4b(id);
	wbl_4b(0L); wbl_4b(0L); wbl_4b(0L); /* No dates */
	wbl_2b(cn_server(p_id)) ;
	errcode=cs_b_size(&cn_capsrc(p_id),&datasize) ;
	if ( errcode!=STD_OK ) {
		wbl_seek(retrack) ;
		if ( def_method==M_IGNORE ) {
			warning("could not determine size of %s,\
 error \"%s\"; file was not archived",
			id_pn(id),err_why(errcode));
			cn_type(p_id)=C_NONE ; aw_none(p_id,id);
		} else {
		warning("could not determine size of %s, error \"%s\";\
 archived capability",
			id_pn(id),err_why(errcode));
			cn_type(p_id)=C_OTHER ; aw_cap(p_id,id);
		}
		return ;
	}
	if ( datasize<DEF_L1 && wbl_tell()+1+datasize+1<BLOCK_SIZE ) {
		/* All fits into current block */
		wbl_1b((int)datasize) ; stillfree=datasize ;
	} else {
		if ( datasize<=255 ) {
			wbl_1b(DEF_L1) ; wbl_1b((int)datasize) ;
		} else if ( datasize<=65535 ) {
			wbl_1b(DEF_L2) ; wbl_2b((int)datasize) ;
		} else {
			/* Assuming 32-bit long */
			wbl_1b(DEF_L4) ; wbl_4b(datasize) ;
		}
		stillfree= BLOCK_SIZE - wbl_tell() ;
		if ( stillfree<4 ) {
			wbl_2b(0) ; wbl_1b(AR_EOD) ; w_end() ;
			stillfree=0 ;
		} else {
			stillfree-= 3 ;
			if ( stillfree>datasize ) stillfree=datasize ;
			wbl_2b((int)stillfree) ;
		}
	}
	/* Now cs_read the stuff */
	totread=0 ;
	if ( !filebuf ) filebuf=getmem(BREAD_SIZE) ;
	while ( totread<datasize ) {
		errcode=cs_b_read(&cn_capsrc(p_id),(b_fsize)totread,
			(char *)filebuf,(b_fsize)BREAD_SIZE,&didread) ;
		if ( errcode!=STD_OK ) {
			warning("could not read (part of) %s, error \"%s\"",
				id_pn(id),err_why(errcode));
			break ;
		}
		if ( didread<=0 ) break ;
		aw_fwr(id,filebuf,totread,(long)didread,stillfree) ;
		stillfree=0 ;
		totread +=didread ;
	}
	if ( totread!=datasize ) {
		warning("file \"%s\", expected %ld bytes, got %ld",
			id_pn(id),(long)datasize,(long)totread);
	}
	if ( datasize && stillfree ) {
		/* Read error while still in definition block */
		/* Forget data definition replace by ...NONE */
		wbl_seek(retrack) ; cn_type(p_id)=C_NONE ; aw_none(p_id,id);
		return ;
	}
}

void
aw_fwr(id,buf,offset,amount,dowrite)
	long		id;
	byte		*buf;
	long		offset;
	long		amount;
	int		dowrite;
{

	long		didwrite ;
	long		canwrite ;
	int		towrite ;

	/* I was planning to make this more stream-like to avoid
	   fragmentation. It proves that fragmentation in the current
	   case is no worse then 0.6% and probably better.
	   So I did not bother.
	*/

	towrite=dowrite ;
	for ( didwrite=0 ; didwrite<amount ; ) {
		if ( !towrite ) {
			/* Only not with first block */
			w_start() ; 
			wbl_1b(AR_CONTENT);
			wbl_1b(AR_DATA);
			wbl_4b(id);
			wbl_4b((long)(offset+didwrite));
			canwrite= BLOCK_SIZE-wbl_tell() -2 ;
			if ( canwrite>amount-didwrite ) {
				canwrite=amount-didwrite ;
			}
			wbl_2b((unsigned)canwrite) ;
		} else {
			canwrite=towrite ;
		}
		wbl_mem((int)canwrite,&buf[didwrite]) ;
		didwrite += canwrite ;
		if ( towrite ) {
			wbl_1b(AR_EOD) ;
			towrite=0 ;
		}
		w_end();
		def_ready=0 ;
	}
}


#ifdef __STDC__
void	aw_cap(struct cap_t *p_id, long id)
#else
void	aw_cap(p_id,id)
	struct cap_t	*p_id;
	long		id;
#endif
{
	errstat		errcode ;
	capability	cap ;

	if ( def_ready && wbl_tell()+1+4*4+1+CAP_SZ+1>=BLOCK_SIZE ) {
		wbl_1b(AR_EOD) ; w_end() ;
		def_ready=0 ;
	}
	if ( !def_ready ) {
		w_start() ; wbl_1b(AR_CONTENT) ; wbl_1b(AR_DEF);
		def_ready=1 ;
	}
	errcode=cs_to_cap(&cn_capsrc(p_id),&cap) ;
	if ( errcode !=STD_OK ) {
		aw_empty(p_id,id) ;
		return ;
	}
	wbl_1b(AR_CAP);
	if ( verbose ) message("archiving capability under %s",id_pn(id));
	wbl_4b(id);
	wbl_4b(0L); wbl_4b(0L); wbl_4b(0L); /* No dates */
#if CAP_SZ>=DEF_L1
	The code needs restructuring, CAP_SZ>=DEF_L1
	Restructure this code
#endif
	wbl_1b(CAP_SZ);
	wbl_cap(&cap);
}

#ifdef __STDC__
void	aw_none(struct cap_t *p_id, long id)
#else
void	aw_none(p_id,id)
	struct cap_t	*p_id;
	long		id;
#endif
{
	if ( def_ready && wbl_tell()+1+4*4+1+1>=BLOCK_SIZE ) {
		wbl_1b(AR_EOD) ; w_end() ;
		def_ready=0 ;
	}
	if ( !def_ready ) {
		w_start() ; wbl_1b(AR_CONTENT) ; wbl_1b(AR_DEF);
		def_ready=1 ;
	}
	if ( verbose ) message("%s has disappeared, not written",
					id_pn(id));
	wbl_1b(AR_NONE);
	wbl_4b(id);
	wbl_4b(0L); wbl_4b(0L); wbl_4b(0L); /* No dates */
	/* No server bytes are written */
	wbl_1b(0); /* No data follows */
}

#ifdef __STDC__
void	aw_empty(struct cap_t *p_id, long id)
#else
void	aw_empty(p_id,id)
	struct cap_t	*p_id ;
	long		id ;
#endif
{
	if ( def_ready && wbl_tell()+1+4*4+1+1>=BLOCK_SIZE ) {
		wbl_1b(AR_EOD) ; w_end() ;
		def_ready=0 ;
	}
	if ( !def_ready ) {
		w_start() ; wbl_1b(AR_CONTENT) ; wbl_1b(AR_DEF);
		def_ready=1 ;
	}
	if ( verbose ) message("%s is inaccessible, not written",
					id_pn(id));
	wbl_1b(AR_EMPTY);
	wbl_4b(id);
	wbl_4b(0L); wbl_4b(0L); wbl_4b(0L); /* No dates */
	wbl_1b(0); /* No data follows */
}

#ifdef __STDC__
void	aw_df(void)
#else
void	aw_df()
#endif
{
	/* Flush out definitions remaining in a block */
	if ( def_ready ) {
		wbl_1b(AR_EOD) ; w_end() ;
		def_ready=0 ;
	}
}

static long
worst_rights(cs)
	capset		*cs;
{
	register int	i ;
	long		rights ;

	rights= ~0L ;

	for ( i = 0; i<cs->cs_final; i++ ) {
		if ( cs->cs_suite[i].s_current ) {
			rights &= cs->cs_suite[i].s_object.cap_priv.prv_rights ;
		}
	}
	return rights ;
}

void
dir_free(p_id)
	struct cap_t		*p_id;
{
	int		j;

	/* Free directory entry list */
	/* First test whether there is anything to be freed */
	if ( !cn_dir(p_id) ) return ;
	for ( j=0 ; j<cn_dir(p_id)->c_count ; j++ ) {
		free(cn_dir(p_id)->c_names[j]) ;
	}
	if ( cn_dir(p_id)->c_names ) {
		free(cn_dir(p_id)->c_names) ;
	}
	for ( j=0 ; j<cn_dir(p_id)->r_count ; j++ ) {
	    if (cn_dir(p_id)->r_entries[j].flags&ENT_VALID){
		free(cn_dir(p_id)->r_entries[j].name) ;
		free((char *)cn_dir(p_id)->r_entries[j].rights) ;
	    }
	}
	if ( cn_dir(p_id)->r_count) {
		free((char *)cn_dir(p_id)->r_entries) ;
	}
	free((char *)cn_dir(p_id));
}

#ifdef MEMDEBUG
put_cleanup(){
	/* release all mem used by put */
	if ( io_type==IO_FILE && !do_stdio ) {
		cs_free(&arch_cs) ;
		cs_free(&arch_dir) ;
	}
	id_cleanup() ;
	if ( filebuf ) free((char *)filebuf) ;
}
#endif

void
w_open(archfile,desc)
	char *archfile ;
	char *desc ;
{
	errstat		errcode;

	ar_type=desc ;
	/* Try to access the archfile name */
	if ( io_type!=IO_FILE || strcmp(archfile,"-")==0 ) {
		openarch(archfile,1,0,0) ;
		if ( io_type==IO_FLOP ) {
			get_disksize(0);
		}
	} else {
		ar_path=archfile ;
		errcode=sp_traverse(SP_DEFAULT, (const char **)&ar_path,
				    &arch_dir) ;
		if ( errcode!=STD_OK ) {
			fatal("can not access %s",archfile) ;
		}
		errcode=sp_lookup(SP_DEFAULT,archfile,&arch_cs) ;
		if ( errcode==STD_OK ) {
			errcode=cs_to_cap(&arch_cs,&arch_cap) ;
			if ( errcode!=STD_OK ) {
				fatal("cs_to_cap failed \"%s\"",err_why(errcode)) ;
			}
		} else {
			errcode=sp_lookup(SP_DEFAULT,BULLET_DEFAULT,&arch_cs) ;
			if ( errcode!=STD_OK ) {
				fatal("please specify a bullet capability with -o") ;
			}
			errcode=cs_goodcap(&arch_cs,&arch_cap) ;
			if ( errcode!=STD_OK ) {
				fatal("please specify a bullet capability with -o") ;
			}
		}
		cs_free(&arch_cs);
	}
}

void
w_close() {
	errstat		errcode;

	if ( io_type==IO_FILE && !do_stdio ) {
		cs_singleton(&arch_cs,&arch_cap);
		errcode=sp_append(&arch_dir,ar_path,&arch_cs,sts_ncols,sts_rights) ;
		if ( errcode==STD_EXISTS ) {
			errcode=sp_replace(&arch_dir,ar_path,&arch_cs) ;
		}
		if ( errcode!=STD_OK ) {
			fatal("could not register archive, error \"%s\"",
				err_why(errcode)) ;
		}
	}
}
