/*	@(#)profiling.h	1.3	96/02/27 10:33:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __MODULE_PROFILING_H__
#define __MODULE_PROFILING_H__

/*
 * Definitions for user-level profiling.
 */

#define PROF_OPC_START	1
#define PROF_OPC_STOP	2

struct profile_parms {
	int    prof_opcode;
	char  *prof_buf;
	int    prof_bufsize;
	char  *prof_pc_low; /* pointer to start of text segment */
	int    prof_scale;
	int    prof_rate;
};


#define	sys_profil	_sys_profil

errstat sys_profil _ARGS(( struct profile_parms * ));
	
#endif /* __MODULE_PROFILING_H__ */
