/*	@(#)sakfunc.h	1.2	94/04/06 17:15:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __SVR_SAKFUNC_H__
#define __SVR_SAKFUNC_H__

#include	"sakbuf.h"

struct sak_job_options;
struct sak_job;

struct sak_job *newjob _ARGS((void));
void freejob _ARGS((struct sak_job *));
void insert_joblist _ARGS((struct sak_job *));
void deletejobfiles _ARGS((objnum, int8));
void remove_job _ARGS((objnum, int8));
errstat sak_submitjob _ARGS((objnum, char *, bufsize, objnum *, port *));
errstat sak_std_destroy _ARGS((objnum, rights_bits));
errstat sak_std_info _ARGS((objnum, rights_bits, char *, bufsize *));
errstat sak_std_age _ARGS((objnum, rights_bits));
errstat sak_listjob _ARGS((objnum, rights_bits, char *, bufsize, bufsize *));
void memfail _ARGS((unsigned));

void remove_from_runlist _ARGS((objnum));
void insert_runlist _ARGS((struct sak_job *));
void handle_jobs _ARGS((void));
void init_server_sleep _ARGS((void));

void handle_req _ARGS((void));
void agejobs _ARGS((void));
struct sak_job *lookup _ARGS((objnum));

void handle_work _ARGS((objnum));
void send_failure _ARGS((struct sak_job_options *, char *, errstat));

int8 schedule _ARGS((struct sak_job *, time_t));
int8 valid_sched _ARGS((int8 **));

void init_pool_sems _ARGS((void));
int8 alloc_free_thread _ARGS((objnum, int));
int8 create_newthread _ARGS((objnum, int));
void wait_for_free_thread _ARGS((objnum, int));

void dosak_exec _ARGS((void));

#endif /* __SVR_SAKFUNC_H__ */
