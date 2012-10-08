/*	@(#)thread.h	1.7	96/02/27 10:26:48 */
/*
 * Copyright 1994 Vrije Universiteit and Stichting Mathematisch Centrum,
 * The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in
 * the top level of the Amoeba distribution.
 */

#ifndef __THREAD_H__
#define __THREAD_H__

#include "_ARGS.h"

#define THREAD_NEWTASK	113
#define THREAD_TRAP	114
#define THREAD_GETLOST	115

#define THREAD_DEFAULT	((int (*) _ARGS(( void ))) 0)
#define THREAD_IGNORE	((int (*) _ARGS(( void ))) 1)

/* What follows used to be in thread.c, but is actually needed elsewhere
 * as well (especially amstdio.h).
 */

/* The maximum number of allocated thread data slots supported by this library.
 */
#define MAX_THREAD_ALLOC	16

/* Some specific thread slots: stack and parameters.
 */
#define THREAD_STACK	0
#define THREAD_PARAM	1

/* Each thread data slot structure specifies the address and size.
 */
struct thread {
	char *m_data;			/* pointer to thread local data */
	int m_size;			/* size of thread local data */
	int (*m_vector) _ARGS(( void ));/* interrupt vector */
};

/* Macros for fast access to the thread local data: */

#ifdef THREAD_NO_OPTIMIZE
#define GETLOCAL(l)	{ if ((l = sys_getlocal()) == 0) l = _main_local; }
#else
#define GETLOCAL(l)	{ if ((l = _thread_local) == (struct thread_data*)(-1))\
				l = sys_getlocal();	\
			  if (l == 0)			\
				l = _main_local;	\
			}
#endif

#define THREAD_DO_ALLOC(ret, index, size) {		\
            struct thread_data *local;			\
            GETLOCAL(local);				\
            if (*(index) > 0 && local->td_tab[*(index)].m_data != 0) { \
                ret = local->td_tab[*(index)].m_data;	\
            } else {					\
                ret = thread_alloc(index, size);	\
            }						\
        }


/* The per thread data.
 */
struct thread_data {
	void (*td_entry) _ARGS(( char *_data, int _size ));
						/* thread entry point */
	struct thread td_tab[MAX_THREAD_ALLOC];	/* thread local data */
};

extern struct thread_data *_thread_local;
extern struct thread_data *_main_local;

/* Prototypes */

#define	thread_newthread		_thread_newthread
#define	thread_exit			_thread_exit
#define	sys_getlocal			_sys_getlocal
#define	thread_alloc			_thread_alloc
#define	thread_realloc			_thread_realloc
#define	threadswitch			_threadswitch
#define	thread_enable_preemption	_thread_enable_preemption
#define	thread_get_max_priority		_thread_get_max_priority
#define	thread_set_priority		_thread_set_priority
#define	exitthread			_exitthread

int thread_newthread _ARGS(( void (*func)(char *_param, int _psize),
				    int _stsize, char *_param, int _psize ));
int thread_exit _ARGS(( void ));
struct thread_data *sys_getlocal _ARGS(( void ));

/* standard thread allocation functions */
char *	thread_alloc _ARGS(( int *_index, int _size ));
char *	thread_realloc _ARGS(( int *_index, int _size ));

/* scheduling routines */
void	threadswitch _ARGS(( void ));
void	thread_enable_preemption _ARGS(( void ));
void	thread_get_max_priority _ARGS(( long * _max ));
void	thread_set_priority _ARGS(( long _new, long * _old ));

/* other system calls */
void	exitthread _ARGS(( long * ));

/* This next one is intended for internal use only */
#define	sys_newthread			_sys_newthread
int	sys_newthread _ARGS(( void (*func)(void), struct thread_data *,
						struct thread_data * ));

#endif /* __THREAD_H__ */
