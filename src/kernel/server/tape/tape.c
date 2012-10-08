/*	@(#)tape.c	1.10	96/02/27 14:21:12 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

/*
 * Tape.c, a tape server; Contains the following routines:
 *
 * tape_init, initializes all tape units present, according to the
 *	      tape_cotnr_tab_type data structure;
 * tape_read, reads from tape;
 * tape_write, writes to tape;
 * tape_eof, writes an end-of-file marker to tape;
 * tape_eom, writes a tape marker to tape;
 * tape_file_skip, skips files;
 * tape_block_skip, skips blocks;
 * tape_erase, writes an eom at the start of the tape;
 * tape_status, returns the tape status;
 * tape_unload, unload the tape.
 *
 * Peter Bosch, 181289, peterb@cwi.nl
 * Greg Sharp, 05/02/91 - Modified so that it can handle more than one tape
 *			  unit and unit numbers larger than 16, such as might
 *			  occur with a SCSI bus.  Took out lots of bugs, too
 *			  numerous to mention and also some histerical raisins.
 *	       06/03/91 - Added tape_wait function for things like ramdisk
 *			  initialization.
 *			Still to be fixed:
 *				Tape controller is not initialized
 *				There is no mechanism for locking the device
 *				for a particular client
 *				Capability rights should be checked
 */

#include "amoeba.h"
#include "cmdreg.h"
#include "stdcom.h"
#include "stderr.h"
#include "machdep.h" /* for vir_bytes */
#include "assert.h"
INIT_ASSERT
#include "tape/conf.h"
#include "tape/tape.h"
#include "file.h"
#include "stdlib.h"
#include "string.h"
#include "module/mutex.h"
#include "module/prv.h"
#include "module/rnd.h"
#include "sys/proto.h"
#include "sys/debug.h"


#define TAPE_NAME	"tape"		/* capability name */
#define	TAPE_SERVER	"TAPE SERVER"	/* name used in error messages */
#define	MAX_TAPE_UNITS	8		/* max #tape units server can manage */

/*
** tape_contr_tab holds all tape entries.
*/
extern tape_contr_tab_type tape_contr_tab[];
extern int num_tape_ctlrs;

/*
** The tape server port... & static variables
*/
static capability	tape_cap;
static int		tape_initialized;
static mutex		tape_init_mtx;

/*
** tape caps are numbered sequentially, and this number will report the
** highest tape unit number + 1
*/
static objnum		Tapenum;
static struct {
    int		t_ctlr;
    int		t_unit;
    capability	t_cap;
}			Tape_table[MAX_TAPE_UNITS];

/*
** tape_init, initialize all tape units present.
**
*/
static void
tape_init()
{
    int		ctlr;
    int		unit;
    errstat	err;
    capability	pub_cap;
    tape_contr_tab_type * tp;

    DPRINTF(1, ("tape_init: called\n"));

    /*
    ** Generate the random port that the tape server will listen to
    ** and fill in public put port that clients know about.
    */
    uniqport(&tape_cap.cap_port);
    priv2pub(&tape_cap.cap_port, &pub_cap.cap_port);

    /*
    ** Initialize the tapes...
    */
    Tapenum = 0;
    for (ctlr = 0; ctlr < num_tape_ctlrs; ctlr++)
    {
	DPRINTF(2, ("tape_init: initializing tape controller %d, '%s'\n", 
				ctlr, tape_contr_tab[ctlr].tape_driver_name));
	tp = &tape_contr_tab[ctlr];
	if (tp->tape_ctlr_init == 0) /* obviously no controller ... */
	    continue;
	DPRINTF(3, ("tape_init: calling ctlr_init function\n"));
	err = (*tp->tape_ctlr_init)(tp->tape_devaddr, tp->tape_ivec);
	if (err != STD_OK)
	{
	    printf("tape_init: init failed for controller %d, (%s)\n",
						ctlr, err_why(err));
	    continue;
	}
	/*
	** Initialize the tape units...
	** If there is no initialization routine then we ignore the
	** controller rather than assume it needs no initializing.
	*/
	if (tp->tape_unit_init == 0)
	    continue;
	DPRINTF(4, ("tape_init: calling unit_init function\n"));

	for (unit = tp->tape_minunit; unit < tp->tape_maxunit; unit++)
	{
	    DPRINTF(2, ("tape_init: initializing unit %d\n", unit));

	    if (Tapenum >= MAX_TAPE_UNITS)
	    {
		printf("%s WARNING: Maximum of %d tape units reached.\n",
						TAPE_SERVER, MAX_TAPE_UNITS);
		return;
	    }

	    /*
	    ** Call unit initialization routine.  If it fails we ignore this
	    ** tape unit.  Otherwise we register it with the directory server.
	    */
	    if ((*tp->tape_unit_init)(tp->tape_devaddr, unit) != STD_OK)
	    {
		DPRINTF(0,
		 ("tape_init: initialization of tape %d, unit %d failed\n",
								ctlr, unit));
		continue;
	    }
	    DPRINTF(4, ("installing tape unit\n"));

	    /*
	    ** Make capability for tape unit and
	    ** make unit known to directory server.
	    */
	    uniqport(&tape_cap.cap_priv.prv_random);
	    (void) prv_encode(&tape_cap.cap_priv, Tapenum,
			    PRV_ALL_RIGHTS, &tape_cap.cap_priv.prv_random);

	    Tape_table[Tapenum].t_ctlr = ctlr;
	    Tape_table[Tapenum].t_unit = unit;
	    Tape_table[Tapenum].t_cap = tape_cap;

	    pub_cap.cap_priv = tape_cap.cap_priv;
	    dirnappend(TAPE_NAME, (int) Tapenum, &pub_cap);
	    Tapenum++;
	}
    }
    DPRINTF(2, ("tape_init: units initialized\n"));
    if (Tapenum <= 0)
    {
	printf("%s: no tape units found on this system\n", TAPE_SERVER);
	mu_lock(&tape_init_mtx);
	mu_lock(&tape_init_mtx);
    }
}

/*
** tape_thread
**	Calls init and starts the (hopefully) endless server loop.
*/
void
tape_thread()
{
    header	hdr;
    bufptr	reqbuf, repbuf, p;
    bufsize	n;
    int		d;
    int		u;
    objnum	tnum;
    rights_bits	rights;
    bufsize	repsize;

    DPRINTF(1, ("tape_thread: started\n"));
    /*
    ** Do the initialization...
    **
    */
    mu_lock(&tape_init_mtx);
    if (!tape_initialized) {
	tape_init();
	tape_initialized = 1;
	wakeup((event) &tape_initialized);   /* see tape_wait() */
    }
    mu_unlock(&tape_init_mtx);

    /*
    ** Miscellaneous initialization...
    **
    */
    if ((reqbuf = (bufptr) aalloc((vir_bytes) T_REQBUFSZ, 0)) == 0)
	panic("tape_thread: Cannot allocate transaction buffer.");

    while (1)
    {
	repbuf = NILBUF;

	hdr.h_port = tape_cap.cap_port;
#ifdef USE_AM6_RPC
	n = rpc_getreq(&hdr, reqbuf, T_REQBUFSZ);
#else
	n = getreq(&hdr, reqbuf, T_REQBUFSZ);
#endif
	if (ERR_STATUS(n))
	{
	    printf("%s: getreq failed: %d\n", TAPE_SERVER, n);
	    panic("");
	}

	/*
	** Check the capability.
	*/
	tnum = prv_number(&hdr.h_priv);
	if (tnum < 0 || tnum > Tapenum ||
	    prv_decode(&hdr.h_priv, &rights,
			    &Tape_table[tnum].t_cap.cap_priv.prv_random) < 0)
	{
	    hdr.h_status = STD_CAPBAD;
	    hdr.h_size   = 0;
#ifdef USE_AM6_RPC
	    rpc_putrep(&hdr, NILBUF, 0);
#else
	    putrep(&hdr, NILBUF, 0);
#endif
	    continue;
	}
	d = Tape_table[tnum].t_ctlr;
	u = Tape_table[tnum].t_unit;
	assert(d >= 0);
	assert(d < num_tape_ctlrs);
	assert(u >= tape_contr_tab[d].tape_minunit);
	assert(u < tape_contr_tab[d].tape_maxunit);

	DPRINTF(2, ("tape_thread: driver %d, unit %d\n", d, u));

	switch (hdr.h_command)
	{
	case STD_INFO:
	case STD_STATUS:
	    /*
	    ** Return info || status string
	    ** NB: both the routines provided by the driver must
	    **     null-terminate their output.
	    */
	    p = repbuf = reqbuf;
	    if (hdr.h_command == STD_INFO)
		p = bprintf(repbuf, reqbuf + T_REQBUFSZ, "& ");

	    p = bprintf(p, reqbuf + T_REQBUFSZ, "%s %d: ",
					tape_contr_tab[d].tape_driver_name, u);

	    if (hdr.h_command == STD_INFO)
		hdr.h_status = (tape_contr_tab[d].tape_info)
		    (tape_contr_tab[d].tape_devaddr, u, p, repbuf + T_REQBUFSZ);
	    else
		hdr.h_status = (tape_contr_tab[d].tape_status)
		    (tape_contr_tab[d].tape_devaddr, u, p, repbuf + T_REQBUFSZ);
		    
	    if (hdr.h_status == STD_OK)
		hdr.h_size = strlen(repbuf);
	    else
		hdr.h_size = 0;
	    repsize = hdr.h_size;
	    break;

	case FSQ_READ: /* In this case we ignore the offset field! */
	case TAPE_READ:
	    /* Read data */
	    repbuf = reqbuf;
	    hdr.h_status = (tape_contr_tab[d].tape_read)
		    (tape_contr_tab[d].tape_devaddr, u, &hdr.h_size, repbuf);
	    if (hdr.h_status != STD_OK &&
			hdr.h_status != (bufsize) TAPE_REC_DAT_TRUNC) {
		hdr.h_size = 0; /* the h_extra field is for FSQ_READ cmd */
		hdr.h_extra = FSE_NOMOREDATA;
	    }
	    else
		hdr.h_extra = FSE_MOREDATA;
	    repsize = hdr.h_size;
	    break;

	case FSQ_WRITE: /* In this case we ignore the offset field! */
	case TAPE_WRITE:
	    /* Write data */
	    hdr.h_status = (tape_contr_tab[d].tape_write)
		    (tape_contr_tab[d].tape_devaddr, u, &hdr.h_size, reqbuf);
	    repsize = 0;
	    break;

	case TAPE_WRITE_EOF:
	    /* Write an eof marker onto the tape */
	    hdr.h_status = (tape_contr_tab[d].tape_write_eof)
					(tape_contr_tab[d].tape_devaddr, u);
	    repsize = 0;
	    break;
	
	case TAPE_FILE_SKIP:
	    /* Skip a number of files */
	    hdr.h_status = (tape_contr_tab[d].tape_file_skip)
			    (tape_contr_tab[d].tape_devaddr, u, hdr.h_offset);
	    repsize = 0;
	    break;

	case TAPE_REC_SKIP:
	    /* Skip a number of files */
	    hdr.h_status = (tape_contr_tab[d].tape_record_skip)
			    (tape_contr_tab[d].tape_devaddr, u, hdr.h_offset);
	    repsize = 0;
	    break;

	case TAPE_REWIND:
	case TAPE_REWIMM:
	    /* Rewind the tape */
	    hdr.h_status = (tape_contr_tab[d].tape_rewind)
					(tape_contr_tab[d].tape_devaddr, u, 
					(hdr.h_command == TAPE_REWIND)? 0: 1);
	    repsize = 0;
	    break;

	case TAPE_ERASE:
	    /* Erase tape */
	    hdr.h_status = (tape_contr_tab[d].tape_erase)
					    (tape_contr_tab[d].tape_devaddr, u);
	    repsize = 0;
	    break;

	case TAPE_LOAD:
	    /* Load the tape drive */
	    hdr.h_status = (tape_contr_tab[d].tape_load)
					    (tape_contr_tab[d].tape_devaddr, u);
	    repsize = 0;
	    break;

	case TAPE_UNLOAD:
	    /* Abort last command */
	    hdr.h_status = (tape_contr_tab[d].tape_unload)
					    (tape_contr_tab[d].tape_devaddr, u);
	    repsize = 0;
	    break;

	case TAPE_FILE_POS:
	case TAPE_REC_POS:
	    if (hdr.h_command == TAPE_FILE_POS)
		hdr.h_status = (tape_contr_tab[d].tape_file_pos)
			    (tape_contr_tab[d].tape_devaddr, u, &hdr.h_offset);
	    else
		hdr.h_status = (tape_contr_tab[d].tape_rec_pos)
			    (tape_contr_tab[d].tape_devaddr, u, &hdr.h_offset);
	    repsize = 0;
	    break;

	default:
	    repsize = 0;
	    hdr.h_status = STD_COMBAD;
	    break;
	}
#ifdef USE_AM6_RPC
	rpc_putrep(&hdr, repbuf, repsize);
#else
	putrep(&hdr, repbuf, repsize);
#endif
    }
}

#ifndef NR_TAPE_THREADS
#define NR_TAPE_THREADS		3
#endif
#ifndef TAPE_STKSIZ
#define TAPE_STKSIZ		0 /* default size */
#endif

void
inittape()
{
    void NewKernelThread();

    register i;

    for(i = 0; i < NR_TAPE_THREADS; i++)
	NewKernelThread(tape_thread, (vir_bytes) TAPE_STKSIZ);
}

/*
** TAPE_WAIT
**
**	So that clients of the tape server in the kernel can wait for the
**	tape server to initialise we provide this routine which won't
**	return until the tape server is initialised.
*/

void
tape_wait()
{
    if (!tape_initialized)
	if (await((event) &tape_initialized, (interval) 0) < 0)
	    panic("tape_wait: got a signal");
}
