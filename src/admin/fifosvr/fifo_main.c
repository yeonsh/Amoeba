/*	@(#)fifo_main.c	1.3	96/02/27 10:13:14 */
/*
 * Copyright 1995 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Original version apr 20 93 Ron Visser
 * Complete reimplementation by Kees Verstoep, june 23 '93.
 *
 * Modified: Gregory J. Sharp, Aug 1995
 *		Bug in publishing super capability and none of the error
 *		messages were useful.
 */

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "fifosvr.h"
#include <stdlib.h>
#include <ampolicy.h>
#include <stdcom.h>
#include <signal.h>
#include <capset.h>
#include <module/buffers.h>
#include <soap/soap.h>
#include <fifo/fifo.h>
#include <bullet/bullet.h>
#include <module/rnd.h>
#include "fifo_impl.h"
#include "fifo_std.h"
#include "fifo_main.h"

static port Null_port;
static port Get_port;		/* Server's get port */
port Super_check;		/* Check field for the super object 0 */
capability Pub_cap;		/* Server's published capability */
static char *Prog_name;

/* The statefile contains a magic string, the getport, checkfield for the
 * supercap and the checkfields for N_fifos fifo objects.
 */
#define CHECK_BYTES	"FIFOFI"
#define N_CHECK_BYTES	(sizeof(CHECK_BYTES) - 1)

static int N_fifos;

static char *State_buf = NULL; /* allocated dynamically */
static size_t State_buf_size;

/* The unmarshalled checkfields for the "stable" fifo objects */
static port *Fifo_check_fields; /* allocated dynamically */

static errstat publish_cap();

static errstat
install_statefile(statefile, buf, size, tryappend, tryreplace)
char *statefile;
char *buf;
int   size;
int   tryappend;
int   tryreplace;
{
    capability bulcap, filecap;
    errstat    err;

    if ((err = name_lookup(DEF_BULLETSVR, &bulcap)) != STD_OK) {
	fprintf(stderr, "%s: cannot look up bullet \"%s\" for statefile (%s)\n",
		Prog_name, DEF_BULLETSVR, err_why(err));
	return err;
    }
    if ((err = b_create(&bulcap, buf, (b_fsize) size, BS_COMMIT | BS_SAFETY,
			&filecap)) != STD_OK) {
	fprintf(stderr, "%s: cannot create statefile (%s)\n",
				Prog_name, err_why(err));
	return err;
    }

    if (tryappend) {
	static rights_bits safe[] = { ALL_RIGHTS, 0, 0};
	capset cs;

	if (!cs_singleton(&cs, &filecap)) {
	    fatal("cannot allocate capset");
	}
	if ((err = sp_append(SP_DEFAULT, statefile, &cs, 3, safe)) == STD_OK) {
	    return STD_OK;
	} else {
	    if (err != STD_EXISTS || !tryreplace) {
		fprintf(stderr, "%s: cannot append \"%s\" (%s)\n",
			Prog_name, statefile, err_why(err));
		return err;
	    }
	}
    }

    if (tryreplace) {
	if ((err = name_replace(statefile, &filecap)) != STD_OK) {
	    fprintf(stderr, "%s: cannot replace \"%s\" (%s)\n",
		    Prog_name, statefile, err_why(err));
	}
    }

    return err;
}

static void
alloc_buffers(nfifo)
int nfifo;
{
    State_buf_size = N_CHECK_BYTES + sizeof(uint32) +
			(1 + 1 + nfifo) * sizeof(port);
    if ((State_buf = (char *) malloc(State_buf_size)) == NULL) {
	fatal("cannot allocate state buf (%ld bytes)", State_buf_size);
    }

    Fifo_check_fields = (port *) malloc((size_t) (nfifo * sizeof(port)));
    if (Fifo_check_fields == NULL) {
	fatal("cannot allocate checkfield array");
    }
}

static int
init_state(doinit, doreplace, nfifo, statefile)
int doinit;
int doreplace;
int nfifo;
char *statefile;
{
    int        i;
    capability cap;
    uint32     nfifo_state;
    b_fsize    size, got;
    errstat    err;
    char      *p, *e;

    if (doinit) {
	/* create and install an fresh statefile */
	uniqport(&Get_port);
	uniqport(&Super_check);
	alloc_buffers(nfifo);
	p = State_buf;
	e = &State_buf[State_buf_size];
	p = buf_put_bytes(p, e, (_VOIDSTAR) CHECK_BYTES, N_CHECK_BYTES);
	p = buf_put_uint32(p, e, (uint32) nfifo);
	p = buf_put_port(p, e, &Get_port);
	p = buf_put_port(p, e, &Super_check);
	for (i = 0; i < nfifo; i++) {
	    p = buf_put_port(p, e, &Null_port);
	}
	if (p == NULL) {
	    fatal("statefile marshalling problem");
	}
	
	if (install_statefile(statefile, State_buf, (p - State_buf),
			      1, doreplace) != STD_OK) {
	    fatal("installation of statefile failed");
	}
    } else {
	/* retrieve previous statefile */
	if ((err = name_lookup(statefile, &cap)) != STD_OK) {
	    fatal("cannot lookup \"%s\" (%s)", statefile, err_why(err));
	}
	if ((err = b_size(&cap, &size)) != STD_OK) {
	    fatal("cannot get size of \"%s\" (%s)", statefile, err_why(err));
	}
	nfifo = ((size - N_CHECK_BYTES - sizeof(uint32)) / sizeof(port)) - 2;
	if (nfifo < MIN_NUM_FIFO || nfifo > MAX_NUM_FIFO) {
	    fatal("invalid number of fifos in statefile (%d)", nfifo);
	}
	alloc_buffers(nfifo);
	if ((err = b_read(&cap, (b_fsize)0, State_buf, size, &got)) != STD_OK){
	    fatal("could not read \"%s\" (%s)", statefile, err_why(err));
	}
	if (got != size) {
	    fatal("read %ld (not %ld) bytes from statefile", got, size);
	}
    }

    /* Now we can unmarshall the statefile */
    if (strncmp(State_buf, CHECK_BYTES, N_CHECK_BYTES) != 0) {
	fatal("bad magic string in statefile \"%s\"", statefile);
    }
    p = State_buf + N_CHECK_BYTES;
    e = &State_buf[State_buf_size];
    p = buf_get_uint32(p, e, &nfifo_state);
    if (nfifo_state != nfifo) {
	fatal("invalid nfifo spec in statefile (%d!=%d)", nfifo_state, nfifo);
    }
    p = buf_get_port(p, e, &Get_port);
    p = buf_get_port(p, e, &Super_check);
    for (i = 0; i < nfifo; i++) {
	p = buf_get_port(p, e, &Fifo_check_fields[i]);
    }
    if (p == NULL) {
	fatal("error unmarshalling statefile");
    }

    /* create Pub_cap from Get_port and Super_check */
    if (prv_encode(&Pub_cap.cap_priv, (objnum) 0,
		   ALL_RIGHTS, &Super_check) != 0) {
	fatal("prv_encode Pub_cap failed!");
    }
    priv2pub(&Get_port, &Pub_cap.cap_port);

    N_fifos = nfifo;
    init_fifo(N_fifos, Fifo_check_fields);
    if (doinit && (publish_cap(doreplace, DEF_FIFOSVR) != STD_OK)) {
	return 0;
    }

    return 1;
}

void
update_statefile()
{
    extern struct fifo_t *fifo_nth_entry();
    struct fifo_t *f;
    char *p, *e;
    int   i;

    /* Put the current check fields in the Statebuf, and write it to disk */
    p = State_buf;
    e = &State_buf[State_buf_size];
    p = buf_put_bytes(p, e, (_VOIDSTAR) CHECK_BYTES, N_CHECK_BYTES);
    p = buf_put_uint32(p, e, (uint32) N_fifos);
    p = buf_put_port(p, e, &Get_port);
    p = buf_put_port(p, e, &Super_check);
    for (i = 0; i < N_fifos; i++) {
	if ((f = fifo_nth_entry(i)) != NULL) {
	    p = buf_put_port(p, e, &f->f_check);
	} else {
	    p = buf_put_port(p, e, &Null_port);
	}
    }

    if (install_statefile(FIFO_STATE_FILE,
			  State_buf, (p - State_buf), 0, 1) != STD_OK) {
	fatal("update of statefile failed");
    }
}

static errstat
publish_cap(doreplace, public_path)
int doreplace;
char *public_path;
{
    static rights_bits supermask[] = { ALL_RIGHTS, USER_RIGHTS, USER_RIGHTS};
    capset cs;
    char *public = "/profile";
    char *super = "/super";
    char *publish;
    errstat err;

    /* We need to publish the fifosvr supercap in the /super version
     * of the default directory ("/profile/cap/fifo/default").
     */
    if (strncmp(public_path, public, strlen(public)) != 0) {
	fatal("fifo cap \"%s\" should start with \"%s\"", public_path, public);
    }
    if ((publish = (char *)malloc(strlen(public_path) + strlen(super))) == 0) {
	fatal("cannot allocate pathname");
    }
    strcpy(publish, super);
    strcat(publish, public_path + strlen(public));

    printf("%s: publishing \"%s\"\n", Prog_name, publish);
    if (!cs_singleton(&cs, &Pub_cap)) {
	fatal("cannot allocate capset");
    }
    if ((err = sp_append(SP_DEFAULT, publish, &cs, 3, supermask)) != STD_OK) {
	if (err != STD_EXISTS || !doreplace) {
	    fatal("cannot append \"%s\" (%s)", publish, err_why(err));
	} else if ((err = name_replace(publish, &Pub_cap)) != STD_OK) {
	    fatal("cannot replace \"%s\" (%s)", publish, err_why(err));
	}
    }

    free((_VOIDSTAR) publish);
    return err;
}

static void
lock_up()
{
    static mutex lock;

    mu_init(&lock);
    for (;;) {
	mu_lock(&lock);
    }
}


static void
usage()
{
    fprintf(stderr, "usage: %s [-i] [-f] [-n nfifo]\n", Prog_name);
    fprintf(stderr, "-i: initialize statefile and supercap before starting\n");
    fprintf(stderr, "-f: replace statefile/supercap if already existing\n");
    fprintf(stderr, "-n nfifo: allow nfifo fifos to be created (default %d)\n",
	    NUM_FIFO);
    exit(1);
}
    
/*ARGSUSED*/
static void
fifo_svr_thd(arg, size)
char *arg;
int   size;
{
    /* call the Ail-generated main loop: */
    extern errstat ml_fifo_svr();
    errstat err;

    err = ml_fifo_svr((port *) arg);
    /* this should never return */
    fatal("fifo thread exited (%s)?!\n", err_why(err));
}

main(argc, argv)
int argc;
char *argv[];
{
    int doinit = 0;
    int doforce = 0;
    int nfifo = NUM_FIFO;	/* default */
    int opt, i;
    extern int optind;

    Prog_name = argv[0];
    while ((opt = getopt(argc, argv, "fin:?")) != EOF) {
	switch (opt) {
	case 'f':
	    doforce = 1;
	    break;
	case 'i':
	    doinit = 1;
	    break;
	case 'n':
	    nfifo = atoi(optarg);
	    if (nfifo < MIN_NUM_FIFO || nfifo > MAX_NUM_FIFO) {
		fatal("invalid number of fifos (min %d, max %d)",
		      MIN_NUM_FIFO, MAX_NUM_FIFO);
	    }
	    break;
	case '?':
	default:
	    usage();
	    break;
	}
    }

    if (optind != argc) {
	usage();
    }

    init_std();
    if (!init_state(doinit, doforce, nfifo, FIFO_STATE_FILE)) {
	fatal("initialisation failed; cannot start server");
    }

    for (i = 0; i < NUM_THREADS; i++) {
	port *p = (port *) malloc(sizeof(port));

	if (p == NULL) {
	    fatal("cannot allocate port for new worker");
	} else {
	    *p = Get_port;
	    if (!thread_newthread(fifo_svr_thd, FIFO_STACK,
				  (char *) p, sizeof(*p))) {
		fatal("could not start %dth thread", i + 1);
	    }
	}
    }

    /* We don't have enough stack by default to act as a worker ourselves. */
    lock_up();

    /*NOTREACHED*/
}

#ifdef __STDC__
#define va_dostart(ap, format)	va_start(ap, format)
#else
#define va_dostart(ap, format)	va_start(ap)
#endif

#ifdef __STDC__
void fatal(char *format, ...)
#else
/*VARARGS1*/
void fatal(format, va_alist) char *format; va_dcl
#endif
{
    /* report fatal error and exit */
    va_list ap;

    va_dostart(ap, format);
    fprintf(stderr, "%s: fatal error: ", Prog_name);
    vfprintf(stderr, format, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}
