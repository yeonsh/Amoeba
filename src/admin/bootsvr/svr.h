/*	@(#)svr.h	1.10	96/03/07 14:08:58 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#include "boot_svr.h"
#include "amtools.h"
#include "module/ar.h"
#include "module/proc.h" /* for process_d typedef*/
#include "thread.h"
#include "_ARGS.h"


#ifndef NULL
#define NULL 0
#endif

#ifndef EOF
#define EOF -1
#endif

#ifndef NILCAP
#define NILCAP ((capability *) NULL)
#endif

#define N_SVR_THREADS	3		/* How many service threads we need */
#define N_TIMER_THREADS	3		/* How many threads to poll & boot */
#define BOOT_STACK	10000		/* Their stacks */
#define TIMER_STACK	20000
#define MAX_CAPENV	10		/* Maximum # caps in env of childs */
#define MAX_OBJ	50			/* Max # objects to manage */
#define MAX_ARG	20			/* Max # arguments to a command */
#define MAX_RETRIES	5		/* How many times a cap may fail */

#define DEFAULT_BOOTRATE 20000		/* Bootrate if not specified */
#define DEFAULT_POLLRATE 10000		/* Pollrate if not specified */

/*
 *	Constants for poll() return:
 */
#define POLL_DOWN	0	/* The process doesn't run */
#define POLL_MAYBE	1	/* Maybe the process runs */
#define POLL_RUNS	2	/* It definitely runs and is ok */
#define POLL_CANNOT	3	/* Doesn't run and won't run */

/*
 *	Do we want this?
 */
#define BOOT_CORE_DUMP_DIR	"/home/boot/dumps"

typedef int bool;

/* Actually, the time fields of the structs obj_rep and boot_data should
 * also be of type Time_t, but that results in name clashes.
 */
typedef long Time_t;

errstat ref_lookup _ARGS((boot_ref *, capability *, obj_rep *));
errstat intr_init _ARGS((void));
void intr_reset _ARGS((void));
void MkMap _ARGS((obj_rep[], int));
errstat capsaid _ARGS((capability *, errstat));
objnum UniqNum _ARGS((void));
void Get_data _ARGS((bufptr, bufptr, obj_rep[], int));
void dumpcore _ARGS((obj_ptr, process_d *, int));
void SetEnv _ARGS((char **));
void TimerInit _ARGS((void));
void TimerThread _ARGS((char *param, int parsize));
void prf _ARGS((char *fmt, ...));
int spawn _ARGS((obj_rep *, boot_cmd *));
int new_obj _ARGS(( void ));
void do_boot _ARGS(( obj_rep * ));


errstat impl_boot_reinit _ARGS((header *, obj_ptr, int *, int *, int *));

Time_t my_gettime _ARGS((void));
Time_t add_time _ARGS((Time_t t0, Time_t incr));
int later _ARGS((Time_t t0, Time_t t1));

extern capability putcap, *get_capp;
extern bool debugging, verbose, closing;
extern bool SaveState, TimePrint;

extern obj_rep objs[MAX_OBJ];

#define obj_in_use(obj)	((obj)->or.or_data.name[0] != '\0')

#define MU_CHECK(mv)	{} /* mutexes no longer contain a thread number */
