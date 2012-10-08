/*	@(#)list.c	1.3	94/04/07 16:07:23 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* STARCH - STore and reSTore ARCHives of graphs.
   module implementing -g option

*/

#include "starch.h"
#include <module/dgwalk.h>


int	doname();
void	name_entry();

int
listnames()
{
	register struct pathlist *flist ;

	for ( flist=files ; flist ; flist=flist->next ) {
                /* First `find' the top level paths */
                name_entry(flist->path,&flist->caps,(capset *)NULL,"");
        }
	return dowalk(doname);
}

int
doname(paths)
	dgw_paths	*paths;
{
	errstat		err;

	err=dgwexpand(paths,name_entry) ;
	if ( err!=STD_OK )
		fatal("Error \"%s\" while traversing paths",err_why(err));
	return 1 ;
}

void
name_entry(path,cs,parent_cs,entry)
	char		*path;
	capset		*cs;
	capset		*parent_cs;
	char		*entry;
{
	register int	res;

	res=testfile(cs,entry) ;
	if ( debug ) {
		printf("%s (%d)\n",path,res);
	} else {
		if ( res!=M_IGNORE ) printf("%s\n",path);
	}
}
