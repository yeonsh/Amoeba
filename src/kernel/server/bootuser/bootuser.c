/*	@(#)bootuser.c	1.8	96/02/27 14:11:35 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * Bootstrap thread (for machines with disks).
 *   
 *   This thread creates the first user process from a file whose
 *   capability is found on a virtual disk.  With code similar to that
 *   found in the bullet server, we scan all vdisks for one with our magic
 *   string.
 *
 *   Changed Mon Oct 8 14:32 by Siebren: took out soap-support.
 *   Modified: Gregory J. Sharp, July 1993 - the createstack() routine assumed
 *					   that page sizes would always be less
 *					   than 32K.  Alas ...
 */

#include <amoeba.h>
#include <stdlib.h>
#include <debug.h>
#include <cmdreg.h>
#include <stdcom.h>
#include <stderr.h>
#include <string.h>
#include <caplist.h>
#include <disk/disk.h>
#include <capset.h>
#include <sp_dir.h>
#include <exception.h>
#include <soap/soap.h>
#include <tod/tod.h>
#include <random/random.h>
#include <boot/bsvd.h>
#include <module/mutex.h>
#include <module/prv.h>
#include <module/rnd.h>
#include <module/proc.h>
#include <module/stdcmd.h>
#include <sys/proto.h>
#include <assert.h>
INIT_ASSERT

/* Debugging support */

#ifdef DEBUG
#define STATIC		/*global*/
#else
#define STATIC		static
#endif

#if !defined(MAIN)
STATIC errstat name_lookup();
#endif /* not MAIN */

/* Parametrizations */

#define BUFSIZE		BLOCKSIZE	/* Buffer size below (>= BLOCKSIZE) */
#define CONSOLE		"tty:00"	/* Console device name */
#define STACKSIZE	(32*1024)	/* Stack size for process */
#define PROGNAME	"-boot"		/* Program name (argv[0]) */

/* Global variables */

/* Capabilities looked up */
static capability Root;			/* . */
static capability Proc;			/* ./proc */
static capability Disk;			/* ./vdisk:xx */
static capability Console;		/* ./tty:00 */
static capability Tod;			/* ./tod */
static capability Random;		/* ./random */
static capability Binary;		/* The process to read */

/* Generated ports & capabilities */
static port Ownerport;			/* process owner private port */
static capability Owner;		/* process owner capability */
static capability Stack;		/* returned by fscreate() */

/* Global buffers */
static header H;			/* general transaction header */
static char Buf[BUFSIZE];		/* general use buffer */

/* Other stuff */
static process_d *Pd;			/* process read by pd_read */
static unsigned long Sp;		/* stack pointer */
static unsigned long Stackstart;	/* start of stack segment */
static long Stacksize;		/* length of stack segment */

/* Program arguments and environments */
static char *Args[] = {			/* command arguments */
	PROGNAME,
	0
};
static char *Envp[2];			/* string environment - will have one
					 * entry for vdisk name.
					 */
static struct caplist Caps[] = {	/* capability environment */
	{"ROOT", &Root},
	{"WORK", &Root},
	{"STDIN", &Console},
	{"STDOUT", &Console},
	{"STDERR", &Console},
	{"TTY", &Console},
	{"TOD", &Tod},
	{"RANDOM", &Random},
	{0, 0}
};


/* Set the Envp to hold the name of the vdisk to be used. */
STATIC void
b_set_env(s1, s2)
char *	s1;
char *	s2;
{
    char * buf;

    buf = (char *) calloc(strlen(s1) + strlen(s2) + 3, 1);
    strcpy(buf, s1);
    strcat(buf, s2);
    Envp[0] = buf;
    Envp[1] = (char *) 0;
}

STATIC void
b_unset_env()
{
    free((_VOIDSTAR) Envp[0]);
    Envp[0] = (char *) 0;
}


/* Subroutine to read one disk block */

STATIC errstat
getblock(blockno)
	disk_addr blockno;
{
	errstat err;
#ifdef MAIN
	err = disk_read(&Disk, L2BLOCK, blockno, (disk_addr)1, Buf);
#else
	extern errstat disk_rw();
	err = ERR_CONVERT(disk_rw(DS_READ, &Disk.cap_priv, L2BLOCK,
				blockno, (disk_addr)1, Buf));
#endif
	if (err != STD_OK)
		printf("bootuser: can't read block %ld (%d)\n", blockno, err);
	return err;
}

/* Subroutine to look up a name */

STATIC errstat
mylookup(name, cap)
	char *name;
	capability *cap;
{
	errstat err;
	if ((err = name_lookup(name, cap)) != STD_OK)
		printf("can't lookup %s (%d)\n", name, err);
	return err;
}

/* Wait until we can go ahead */

STATIC void
greenlight()
{
#ifndef MAIN
	void disk_wait();	/* from virtual disk server */

	DPRINTF(2, ("bootuser: waiting for green light ...\n"));
	disk_wait();
	DPRINTF(2, ("bootuser: GO.\n"));
#endif
}

/* Restrict a capability.
   This is necessary since the Root capability has some rights bits set
   that make it into an invalid capability when presented by a user
   process. */

STATIC void
striprights(prv, mask)
	private *prv;
	rights_bits mask;
{
	port random;
	objnum obj;
	rights_bits rights;
	
	obj = prv_number(prv);
	random = prv->prv_random;
	if (prv_decode(prv, &rights, &random) != 0)
		printf("bootuser: can't strip rights (prv_decode failed)\n");
	else if (prv_encode(prv, obj, rights & mask, &random) != 0)
		printf("bootuser: can't strip rights (prv_encode failed)\n");
}

/* Look up global capabilities */

STATIC errstat
getcaps()
{
	errstat err;
	
	DPRINTF(2, ("bootuser: getting capabilities ...\n"));
	
	if ((err = mylookup("", &Root)) != STD_OK)
		return err;
	striprights(&Root.cap_priv, (rights_bits)0x3f);
	if ((err = mylookup(PROCESS_SVR_NAME, &Proc)) != STD_OK)
		return err;
	if ((err = mylookup(CONSOLE, &Console)) != STD_OK)
		return err;
	if ((err = mylookup(TOD_SVR_NAME, &Tod)) != STD_OK)
		return err;
	if ((err = mylookup(RANDOM_SVR_NAME, &Random)) != STD_OK)
		return err;
	uniqport(&Ownerport);
	priv2pub(&Ownerport, &Owner.cap_port);
	return STD_OK;
}

/* Find the vdisk with our magic number in block 0 */

STATIC errstat
finddisk()
{
	capset cs;
	SP_DIR *dd;
	struct sp_direct *dp;
	errstat err;
	errstat ret;
	
	DPRINTF(2, ("bootuser: looking for vdisks ...\n"));
	
	if (!cs_singleton(&cs, &Root)) {
		DPRINTF(2, ("bootuser: cs_singleton failed\n"));
		return STD_NOMEM;
	}
	if ((err = sp_list(&cs, &dd)) != STD_OK) {
		DPRINTF(2, ("bootuser: sp_opendir failed (%d)\n", err));
		return err;
	}
	cs_free(&cs);
	ret = STD_NOTFOUND;
	while ((dp = sp_readdir(dd)) != 0) {
		DPRINTF(2, ("bootuser: got '%s'\n", dp->d_name));
#ifdef COLDSTART
		/* Only look at RAM disks when coldstarting */
		if (strncmp(dp->d_name, "vdisk:8", 7) != 0)
			continue; /* Not a vdisk */
#else
		if (strncmp(dp->d_name, "vdisk:", 6) != 0)
			continue; /* Not a vdisk */
#endif /* COLDSTART */
		if ((err = mylookup(dp->d_name, &Disk)) != STD_OK)
			continue; /* Huh? */
		if ((err = getblock((disk_addr)0)) != STD_OK)
			continue; /* Read failed */
		if (strcmp(((struct bsvd_head *)Buf) -> magic,
						BOOT_MAGIC) == 0) {
			b_set_env("BOOT_VDISK=", dp->d_name);
			printf("bootuser: using %s\n", dp->d_name);
			Binary = ((struct bsvd_head *)Buf) -> binary;
			ret = STD_OK;
			break;
		}
	}
	sp_closedir(dd);
	return ret;
}


/*
 * The "stack" consists of arguments that go at bufstart and the rest of
 * the stack frame that begins at "Stackstart".  The two had better be
 * different.  NB. Stack start is the start of the stack segment but the
 * top of the stack not the start or base of the stack.
 */
STATIC errstat
createstack()
{
	errstat		err;
	int		clickshift;
	int		vmlo, vmhi; /* In clicks */
	unsigned long	bufstart;   /* VM address where stack buffer starts */
	header		hdr;
	
	DPRINTF(2, ("bootuser: create stack\n"));
	if ((err = pro_getdef(&Proc, &clickshift, &vmlo, &vmhi)) != STD_OK)
		return err;
	
	/*
	 * Compute values in clicks
	 * NB.  On the sparc we'll only get 1 click since CLICKSIZE is
	 *      bigger than STACKSIZE
	 */
	Stacksize = ((STACKSIZE - 1) >> clickshift) + 1;
	Stackstart = vmhi - Stacksize;

	/* Convert to bytes */
	Stackstart <<= clickshift;
	Stacksize <<= clickshift;
	/*
	 * bufstart is where the environment has to go.  We put it at the top
	 * of the stack.  The following will work on 2's complement machines.
	 */
	bufstart = (vmhi << clickshift) - BUFSIZE;
	assert(bufstart > Stackstart);
	
	Sp = buildstack(Buf, (long) BUFSIZE, bufstart, Args, Envp, Caps);
	if (Sp == 0)
		return STD_SYSERR;
	/* Create a segment on the current host - we don't use ps_segcreate
	 * because for that we need an exec capability which we don't have
	 */
	hdr.h_port = Proc.cap_port;
	hdr.h_offset = Stacksize;
	err = usr_segcreate(&hdr, (capability *) 0);
	Stack.cap_port = hdr.h_port;
	Stack.cap_priv = hdr.h_priv;
	if (err == STD_OK)
		err = ps_segwrite(&Stack, (long) (bufstart - Stackstart), Buf,
							    (long) BUFSIZE);
	b_unset_env(); /* Free the memory used by the Envp */
	DPRINTF(2, ("bootuser: create stack returned %x\n", err));
	return err;
}

STATIC errstat
readprocess()
{
	segment_d *sd;
	int i;
	
	DPRINTF(2, ("bootuser: readprocess\n"));
	/*
	 * The file server with the binary we must start may not be up yet.
	 * Therefore we poll the capability until we get an error status that
	 * suggests that the file server is present or that the capability is
	 * bogus.  We don't try forever since it isn't worth it.  Five minutes
	 * is enough.
	 */
	for (i = 0; i < 20 ; i++)
	{
	    errstat err;
	    int dummy;

	    err = std_info(&Binary, (char *) 0, 0, &dummy);
	    if (err == STD_OK || err == STD_OVERFLOW)
		break;
	    if (err != RPC_FAILURE && err != RPC_NOTFOUND)
	    {
		printf("bootuser: std_info of binary failed (%d)\n", err);
		return err;
	    }
	    /* wait 15 seconds and try again */
#ifdef MAIN
	    sleep(15)
#else
	    await((event) 0, (interval) 15000);
#endif
	}
	Pd = pd_read(&Binary);
	if (Pd == 0) {
		printf("bootuser: pd_read failed\n");
		return STD_SYSERR;
	}
	Pd->pd_owner = Owner;
	DPRINTF(2, ("%d segments\n", Pd->pd_nseg));
	/* This is copied more or less from exec_file() */
	for (sd = PD_SD(Pd), i = Pd->pd_nseg; --i >= 0; ++sd) {
		if (NULLPORT(&sd->sd_cap.cap_port)) {
			if (sd->sd_offset != 0) {
				sd->sd_cap = Binary;
			}
			else if (sd->sd_type & MAP_SYSTEM) {
				sd->sd_cap = Stack;
				sd->sd_addr = Stackstart;
				sd->sd_len = Stacksize;
				sd->sd_type = MAP_GROWDOWN | MAP_TYPEDATA |
					MAP_READWRITE | MAP_AND_DESTROY;
			}
		}
		DPRINTF(2, (
		  "%d: off %x, ad %x, len %x, addr+len %x, type %x\n",
		  Pd->pd_nseg - i - 1,
		  sd->sd_offset,
		  sd->sd_addr,
		  sd->sd_len,
		  sd->sd_addr + sd->sd_len,
		  sd->sd_type));
	}
	((thread_idle *)(PD_TD(Pd)+1)) -> tdi_sp = Sp;
	return STD_OK;
}

STATIC errstat
startprocess()
{
	capability dummy;
	errstat err;

	if ((err = pro_exec(&Proc, Pd, &dummy)) != STD_OK) {
		printf("bootuser: pro_exec failed (%d)\n", err);
	} else {
		if ((err = pro_setcomment(&dummy, "Daemon: boot")) != STD_OK)
		    printf("bootuser: pro_exec failed (%d)\n", err);
	}
	return err;
}

STATIC errstat
waitforprocess()
{
	static char infobuf[] = "bootuser thread"; /* For STD_INFO */
	errstat err;
	
	for (;;) {
		H.h_port = Ownerport;
#ifdef USE_AM6_RPC
		err = ERR_CONVERT(rpc_getreq(&H, Buf, BUFSIZE));
#else
		err = ERR_CONVERT(getreq(&H, Buf, BUFSIZE));
#endif
		if (err == RPC_ABORTED)
			continue; /* Interrupted -- try again */
		if (ERR_STATUS(err)) {
			printf("bootuser: getreq error %d\n", err);
			return err;
		}
		if (H.h_command == STD_INFO) {
			H.h_status = STD_OK;
			H.h_size = sizeof infobuf;
#ifdef USE_AM6_RPC
			rpc_putrep(&H, infobuf, sizeof infobuf);
#else
			putrep(&H, infobuf, sizeof infobuf);
#endif
		}
		else if (H.h_command != PS_CHECKPOINT) {
			DPRINTF(2, ("bootuser: got bad request %d\n",
							    H.h_command));
			H.h_status = STD_COMBAD;
#ifdef USE_AM6_RPC
			rpc_putrep(&H, NILBUF, 0);
#else
			putrep(&H, NILBUF, 0);
#endif
		}
		else {
			switch (H.h_extra) {
			case TERM_NORMAL:
			    printf("bootuser: process exit %d\n", H.h_offset);
			    break;
			case TERM_STUNNED:
			    printf("bootuser: process got stun %d\n",
								H.h_offset);
			    break;
			case TERM_EXCEPTION:
			    printf("bootuser: process got exception %s\n",
							exc_name(H.h_offset));
			    break;
			default:
    printf("bootuser: process sent weird checkpoint, cause %d, detail %ld\n",
							H.h_extra, H.h_offset);
			    break;
			}
			H.h_status = STD_SYSERR;
#ifdef USE_AM6_RPC
			rpc_putrep(&H, NILBUF, 0);
#else
			putrep(&H, NILBUF, 0);
#endif
			return STD_OK;
		}
	}
	/*NOTREACHED*/
}

/* Called when we're done.  In the kernel, we can't exit so we must loop. */

STATIC void
done()
{
#ifndef MAIN
	static mutex mu;
	
	for (;;)
		mu_lock(&mu);
#endif
}

/* Control procedure.  In the kernel this is started by initbootuser(). */

static void
bootuser _ARGS((void))
{
	greenlight();
	if (getcaps() != STD_OK)
		printf("bootuser: can't get all required capabilities\n");
	else if (finddisk() != STD_OK)
		printf("bootuser: no bootable vdisk partition found\n");
	else if (createstack() != STD_OK)
		printf("bootuser: no stack created\n");
	else if (readprocess() != STD_OK)
		printf("bootuser: no process read\n");
	else {
		printf("bootuser: starting user process ...\n");
		if (startprocess() != STD_OK)
			printf("bootuser: no process started\n");
		else {
			printf("bootuser: process started; wait ...\n");
			(void) memset((_VOIDSTAR) &Stack, 0, CAPSIZE);
			if (waitforprocess() != STD_OK)
			    printf("bootuser: wait failed!\n");
		}
	}
	if (!NULLPORT(&Stack.cap_port))
		(void) std_destroy(&Stack);
	done();
}

#ifdef MAIN

/* Main program, for testing outside the kernel. */

main(argc, argv)
	int argc;
	char **argv;
{
	errstat err;
	
	/* Use printf, not fprintf(stderr), because in the kernel we also
	   only have printf, so the rest of the code has to use printf. */
	
	if (argc != 2 || argv[1][0] == '-') {
		printf("usage: bootuser machine-directory\n");
		exit(2);
	}
	if ((err = cwd_set(argv[1])) != STD_OK) {
		printf("bootuser: can't cwd_set to %s (%s)\n",
			argv[1], err_why(err));
		exit(1);
	}
	bootuser();
	exit(0);
}

#else /*MAIN*/

#ifndef BOOTUSER_STKSIZ
#define BOOTUSER_STKSIZ	0 /* default size */
#endif

void
initbootuser() {

	NewKernelThread(bootuser, (vir_bytes) BOOTUSER_STKSIZ);
}

/* A function that the kernel library doesn't have. */
/* XXX There is one in libkernel.a but it needs dir_lookup which isn't. */

STATIC errstat
name_lookup(name, cap)
	char *name;
	capability *cap;
{
	extern capset _sp_rootdir;
	capset cs;
	errstat err;
	if ((err = sp_lookup(&_sp_rootdir, name, &cs)) == STD_OK) {
		*cap = cs.cs_suite[0].s_object;
		cs_free(&cs);
	}
	return err;
}

#endif /*!MAIN*/
