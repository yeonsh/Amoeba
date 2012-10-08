/*	@(#)ampolicy.h	1.11	96/03/26 11:23:48 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __AMPOLICY_H__
#define __AMPOLICY_H__

/*
**	This file contains policy decisions relevant to the management of
**	the system servers.  It is here to centralise the adjustment of
**	system setup before compiling and building the system.
*/


/*
**	General tree structure.
**
**	Most shared capabilities are found somewhere in a tree under a
**	directory called /super (by sysadmins).
**	To ease personalization, programs refer to it as /profile.
**	/profile is actually a per-user directory which contains copies of
**	the capabilities found at the top level in /super, as well as
**	capabilities and directories that differ per user.
*/

#define HOST_DIR	"/profile/hosts"	/* list of *all* hosts */
#define HOST_SUPER_DIR	"/super/hosts"		/* super user's version */
#define POOL_DIR	"/profile/pool"		/* host groups used as pool */
#define	DEFAULT_HOME	"/home"			/* users' home directory */


/* SOAP SERVER */
/***************/

#define	STDAGE_TIMER	24		/* #hours between STD_AGE commands */
/* (Shouldn't this be a parameter choosable at setup time?) */


/* BULLET SERVER */
/*****************/

#define	DEF_BULLETSVR	"/profile/cap/bulletsvr/default"
					/* default bullet server */

/* PRINTBUF SERVER */
/*******************/

#define DEF_PRINTBUFNAME	"printbuf"
				/* separate printbuf server in the kernel */

/* PROFILE SERVER */

#define DEF_PROFILENAME		"profile"
				/* profile server in the kernel */

/* SYSTEM SERVER */
/*****************/

#define DEF_SYSSVRNAME	"sys"
				/* system server in the kernel */

/* IOP SERVER */
/**************/

#define	DEF_IOPSVRNAME	"iop"
				/* IOP server in the kernel */

/* TAPE SERVER */
/***************/

#define	DEF_TAPESVR	"/profile/cap/tapesvr/default"  /* Default tape */

/* TOD SERVER */
/**************/

#define	DEF_TODSVR	"/profile/cap/todsvr/default"
					/* default time of day server */


/* RANDOM NUMBER SERVER */
/************************/

#define	DEF_RNDSVR	"/profile/cap/randomsvr/default"
					/* default random number svr */

/* SESSION SERVER */
/******************/

#define DEF_SESSION_PROC_DIR	"/dev/proc"	/* publishing process caps */
#define DEF_SESSION_CAP		"/dev/session"	/* session super cap name */
#define DEF_NULLSVR		"/dev/null"	/* Posix /dev/null emulation */
#define DEF_CORE_DIR		"/dev/dead"	/* core files stored here */
#define	DEF_SESSION_CONSOLE	"/dev/console"	/* default console */

/* RUN SERVER (LOAD BALANCING) */
/*******************************/

/* Default pool capability of heterogeneous runsvr (for "findhost" requests,
 * set parameters, load info, etc.)
 */
#define DEF_RUNSVR_POOL	"/profile/pool/.run"

/* Default run server super capability (for pd_create requests). */
#define DEF_RUNSERVER	"/profile/cap/runsvr/default"

/* Directory to do the publishing in: */
#define PUB_RUNSVR_DIR	"/super/cap/runsvr"

/* Directory to store the runsvr's pool directory adminstration */
#define DEF_RUN_ADMIN	"/super/admin/module/run"

/* TCP/IP SERVER */
/*****************/

#define ETH_SVR_NAME	"/profile/cap/ipsvr/eth"	/* ethernet server */
#define IP_SVR_NAME	"/profile/cap/ipsvr/ip"		/* ip server */
#define TCP_SVR_NAME	"/profile/cap/ipsvr/tcp"	/* tcp server */
#define UDP_SVR_NAME	"/profile/cap/ipsvr/udp"	/* udp server */

/* X WINDOWS */
/*************/

#define DEF_XDIR	"/profile/module/x11"		/* X directory */
#define DEF_XSVRDIR	"/profile/cap/x11svr"		/* all servers */

/* SAK SERVER */
/**************/

#define SAK_PUBLISH_DIR_DEFAULT		"/super/cap/saksvr"
#define SAK_PUB_NAME_DEFAULT		"default"
#define SAK_PUB_EXECNAME_DEFAULT        "execdefault"
#define DEF_SAKSVR			"/profile/cap/saksvr/default"
#define DEF_EXECSVR			"/profile/cap/saksvr/execdefault"
#define SAK_MASTERFILE			"sak_masterfile"
#define	SAK_USERDIR			"/home/sak"

/* BOOT SERVER */
/***************/

/* Capability for clients to contact the boot server */
#define DEF_BOOTSERVER  "/%s/cap/bootsvr/default"

/* For allo and friends */
#define DEF_SHELL       "/bin/sh"

/* OBJECT MANAGER */
/******************/
#define	DEF_OMSVRDIR	"/super/cap/omsvr"
#define DEF_OMDATADIR	"/super/admin/module/om"

/* FIFO SERVER */
/******************/
#define	DEF_FIFOSVR	"/profile/cap/fifosvr/default"
#define FIFO_STATE_FILE	"/super/admin/module/fifo/state"

/* RELIABLE RPC PACKAGE */
/************************/
#define	RRPC_GRP_DIR	"/profile/cap/rrpc/groups"
#define	RRPC_CTX_DIR	"/profile/cap/rrpc/ctx"

/* LOGIN */
/*********/

#define	ENVIRONFILE	"Environ"
#define	MAX_LOGNAME_SZ	20
#define	MAX_PASSWD_SZ	8

/* MISCELLANEOUS DEFINES */
/*************************/
#define TFTPBOOT_DIR    "/super/admin/tftpboot"

/* Defines for ethers & hosts files for tcpip/rarp/tftp & friends */
#define	ETHERS_FILE	"/etc/ethers"
#define	HOSTS_FILE	"/etc/hosts"

/* The place the kernels are kept under Amoeba */
#define	DEF_KERNELDIR	"/super/admin/kernel"

/* Where to look for users' directories */
#define	DEF_USERDIR	"/profile/group/users"

/* The (ip) hostname of the amoeba system */
#define	HOSTNAME_FILE	"/etc/Hostname"

#endif /* __AMPOLICY_H__ */
