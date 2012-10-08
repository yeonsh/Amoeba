/*	@(#)run.cls	1.3	96/02/27 10:28:12 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "cmdreg.h"

class run[RUN_FIRST_COM .. RUN_LAST_COM] {

    const PD_BUFSIZE = 6000;	/* for process descriptor buffer */
    const PD_HOSTSIZE = 16;	/* for hostname buffer */
    const PD_ARCHSIZE = 8;	/* for arch name buffer */

    /* return the best host in a pool given one pd */
    run_get_exec_cap(*,
	var in  capability  proccap,	/* process svr super cap of host */
	out	capability  execcap,	/* exec cap of host chosen */
	out	char	    arch_name[PD_ARCHSIZE] /* name of host chosen */
    );

    /* return the best host in a pool given a set of pd's */
    run_multi_findhost(*,
	in  int		pd_num,			   /* number of pds in pdbuf */
	in  char	pd_buf[pd_len:PD_BUFSIZE], /* buffer with >= 1 pd */
        in  int		pd_len,          	   /* length of pd buffer */
	out int		pd_which,		   /* which pd was chosen */
	out char	host_name[PD_HOSTSIZE],	   /* name of host chosen */
	out capability	host_cap		   /* cap of host chosen */
    );

    /* Create a new pooldir object, and return a new capability for it.
     * The pooldir object will be associated to directory `pool_dir'.
     * As usual, the std_touch and std_age commands as performed by `om'
     * are used to implement garbage collection of pooldir objects.
     * Additionally, the std_touch command will cause a rescan of the
     * corresponding pooldir.
     * Note there's no pool_create_archdir() command, because pools
     * have become heterogeneous, and the arch dirs cannot be
     * adressed separately anymore.
     */
    run_create(*,
	var in  capability pool_dir,
	out     capability pool_object
    );

};
