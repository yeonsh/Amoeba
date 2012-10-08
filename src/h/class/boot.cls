/*	@(#)boot.cls	1.3	94/04/06 15:48:15 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include <ailamoeba.h>
#include <class/std.cls>
#include <class/stdrights.cls>
#include <class/checkpoint.cls>
#include <class/proswappro.cls>

/*
 *	Manifest constants; overridable
 */
#ifndef MANIFEST_MAX_PATH
#define MANIFEST_MAX_PATH	50
#endif
#ifndef MANIFEST_MAX_IDENT
#define MANIFEST_MAX_IDENT	20
#endif

/*
 *	Class definition for the bootserver
 */
class boot [BOOT_FIRST_COM .. BOOT_LAST_COM] {
    inherit stdinfo, stdstatus, stdrestrict,
	bullet_rgts, checkpoint, proswapproc;

    /*
     *	A boot_ref points to a capability, perhaps
     *	indirectly using the directory server.
     *	If the path is empty, 'cap' is the boot_refd
     *	capability. Otherwise it is the directory from
     *	which to start dir_lookup's to find it.
     */
    const MAX_PATH = MANIFEST_MAX_PATH;
    typedef struct {
	capability cap;
	char path[MAX_PATH];
    } boot_ref;

    /*
     *	I guess the server will normally boot a process by executing
     *	its binary, and stop it by stunning it.
     *	Alternatively, you can specify programs to do this for you.
     */

    typedef struct {		/* To store the capability environment */
	int from_svr;		/* Inherit from server? */
	boot_ref cap_ref;	/* Where to look it up if not inherited */
	char *cap_name;		/* Its name */
    } boot_caps;

    typedef struct {		/* To store a command */
	boot_ref executable;	/* Executable file */
	int stack;		/* Stack size */
	boot_caps *capv;	/* Capability environment */
	int ncaps;		/* How many there are */
	char **envp;		/* String environment */
	char **argv;		/* Argument vector */
    } boot_cmd;

    /*
     *	Flags of an incarnating process:
     */
    const BOOT_POLLREF = 0x02;		/* Use pollcap instead of proccap */
    const BOOT_DOWNCMD = 0x04;		/* Bring down using another program */
    const BOOT_INACTIVE = 0x08;		/* Don't take actions */
    const BOOT_ONPROCSVR = 0x10;	/* "machine" points to the procsvr */

    typedef char ident[MANIFEST_MAX_IDENT];

    /*
     *	Visible state of an incarnating process:
     */
    typedef struct {
	ident name;			/* Logical name */
	int flags;			/* See above constants */
	int status;			/* Condition of process */
	long pollrate;			/* How often to spend time on it */
	long bootrate;			/* How long booting may take */
	boot_ref machine;		/* Where it should run */
	boot_cmd bootcmd;		/* How to boot instead of exec */
	boot_ref pollcap;		/* What to poll instead of proccap */
	ident depname;			/* Depending on which, if any */
    } boot_data;

    /*
     *	Rpc-interface:
     */

    boot_reinit(*,		/* Re-read configuration */
	out int last_line_read,	/* The parser stops at the first error */
	out int success,	/* 0: an error occured 1: all well */
	out int n_conf		/* How many configurations there are */
    ) rights BS_RGT_ADMIN;

    /* Gracefully terminate the server */
    boot_shutdown(*) rights BS_RGT_ADMIN;

    /* Switch output device */
    boot_setio(*,
	var capability io
    ) rights BS_RGT_ADMIN;

    /* Turn on / off the BOOT_INACTIVE flag */
    boot_conf(*,
	char confname[MANIFEST_MAX_IDENT], int on
    ) rights BS_RGT_ADMIN;

    /* Much like runctl - '?' returns what's supported */
    boot_ctl(*,
	char option, out char msg[len : MAX_STD_STATUS_LEN], out int len
    ) rights BS_RGT_ADMIN;
};
