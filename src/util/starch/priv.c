/*	@(#)priv.c	1.5	96/02/27 13:16:08 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* STARCH - STore and reSTore ARCHives of graphs.
   module implementing -g option

*/

#include "starch.h"
#include "amoeba.h"
#include "cmdreg.h"
#include "stderr.h"
#include "capset.h"
#include <server/soap/soap.h>
#include <module/goodport.h>
#include <module/dgwalk.h>


int	priv_do();
void	priv_entry();
void	cnvrt_entry();

int
dopriv()
{
	register struct pathlist *flist ;

	for ( flist=files ; flist ; flist=flist->next ) {
                /* First `find' the top level paths */
                priv_entry(flist->path,&flist->caps,(capset *)NULL,"");
        }
	return dowalk(priv_do);
}

int
priv_do(paths)
	dgw_paths	*paths;
{
	errstat		err;

	err=dgwexpand(paths,priv_entry) ;
	if ( err!=STD_OK )
		warning("Error \"%s\" while traversing .../%s",
			err_why(err),paths->entry);
	return 1 ;
}

void
priv_entry(path,cs,parent_cs,entry)
	char		*path;
	capset		*cs;
	capset		*parent_cs;
	char		*entry;
{
	register int	res;

	if ( !parent_cs ) return ;
	res=testfile(cs,entry) ;
	if ( debug ) {
		printf("%s (%d)\n",path,res);
	}
	if ( res==M_IGNORE ) return ;
	cnvrt_entry(parent_cs, cs, path, entry);
}

/*
 * Tree convert
 *
 *	This routine is used to convert a soap directory entry's capabilities
 *	to use a public port.  This routine should be incorporated in a
 *	program such as starch or brol to traverse the directory graph and
 *	convert all the directory entries.
 *
 *	It assumes that the parameters it gets are valid and sensible!
 *	The best way to treat this is to restrict to a set of well known
 *	ports, namely the servers you want to convert.
 */
void
cnvrt_entry(dir_cs, file_cs, pathname, capname)
capset *	dir_cs;		/* Capability set for a directory */
capset *	file_cs;	/* File capset */
char *		pathname;	/* Name of the whole path */
char *		capname;	/* Name of an entry in the directory */
{
    int		sp_err;	/* for soap errors */
    capset	new_cs;
    short	i;
    suite *	s;
    capability  cap ;
    capability *cap_ptr ;
    sp_entry	entry ;
    register struct serverlist	*slist ;
    int		do_it ;
    int		any_done=0 ;


    if ( !cs_copy(&new_cs,file_cs) ) {
	fatal("cnvrt_entry: copy of capset for %s failed",pathname);
    }
    /* For each capability in the capset we convert its port to a put-port */
    for (i = 0; i < new_cs.cs_final; i++)
    {
	s = &new_cs.cs_suite[i];
	if (s->s_current)
	{
	    do_it=0 ;
	    for ( slist=priv_servers ; slist ; slist=slist->next ) {
		if(SAME_SERVER(&slist->server,&s->s_object)) {
		    do_it=1 ; break ;
		}
	    }
	    if ( !do_it ) continue ;
	    any_done++ ;
	    /* -f is intended for real priv2pub action, for bullet files
	       Without -f semi action is taken. The entry is replaced
	       by the capability read. Which will convert all soap
	       cap's in the tree because soap gives putports even though
	       soap getports are stored in the tree.
	     */
	    if ( force ) {
		priv2pub(&s->s_object.cap_port, &s->s_object.cap_port);
	    }
	}
    }

    if ( any_done==0 ) {
	cs_free(&new_cs) ;
	return ;
    }
    if ( verbose && !debug ) {
	printf("%s\n",pathname);
    }
    /* Put the capset back! */
    entry.se_name= capname ;
    entry.se_capset= *dir_cs ;
    sp_err=cs_to_cap(file_cs,&cap) ;
    if (sp_err != STD_OK)
    {
	warning("cnvrt_entry: cs_to_cap of %s failed (%s)",
					    pathname, err_why(sp_err));
	cs_free(&new_cs);
	return;
    }
    cap_ptr = &cap ;
    sp_err = sp_install(1, &entry, &new_cs, &cap_ptr) ;
    cs_free(&new_cs);
    if (sp_err != STD_OK)
    {
	warning("cnvrt_entry: replace of %s failed (%s)",
					    pathname, err_why(sp_err));
	return;
    }
}
