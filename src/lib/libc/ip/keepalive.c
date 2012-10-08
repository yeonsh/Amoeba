/*	@(#)keepalive.c	1.3	96/02/27 11:08:34 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
libip/tcpip/keepalive.c
*/

#include <assert.h>

#include <amtools.h>
#include <semaphore.h>
#include <thread.h>

#include <server/ip/tcpip.h>

#if DEBUG
#define where()		printf("%s %d:\n        ", __FILE__, __LINE__)
#define endWhere()	{/* nothing */}
#endif

#define KEEPALIVE_STACK	10240

static mutex keepalive_mutex;	/* statically initalised */

static void keepalive_thread _ARGS(( char *argp, int args ));

typedef struct keepalive
{
	capability ka_cap;
	int ka_time;
	struct keepalive *ka_next;
} keepalive_t;

static keepalive_t *keepalive_list;
static semaphore keepalive_semaphore;
static keepalive_inited= 0;

errstat tcpip_keepalive_cap(cap)
capability *cap;
{
	keepalive_t *keepalive, *k_ptr;
	int result;

#if DEBUG & 2
 { where(); printf("tcpip_keepalive_cap called with 0x%x\n", cap); endWhere(); }
#endif
	keepalive= (keepalive_t *)malloc(sizeof(*keepalive));
	if (!keepalive)
	{
		return STD_NOSPACE;
	}

	keepalive->ka_cap= *cap;
	keepalive->ka_time= 0;

#if DEBUG & 2
 { where(); printf("doing mu_lock on keepalive_mutex\n"); endWhere(); }
#endif
	mu_lock(&keepalive_mutex);
	if (!keepalive_inited)
	{
		sema_init(&keepalive_semaphore, 0);
		keepalive_list= 0;
		result= thread_newthread(keepalive_thread, KEEPALIVE_STACK,
			(char *)0, 0);
		if (!result)
		{
#if DEBUG
 { where(); printf("unable to start keepalive_thread\n"); endWhere(); }
#endif
			free(keepalive);
#if DEBUG
 { where(); printf("doing mu_unlock on keepalive_mutex\n"); endWhere(); }
#endif
			mu_unlock(&keepalive_mutex);
			return STD_NOSPACE;
		}
		keepalive_inited= 1;
	}
	for (k_ptr= keepalive_list; k_ptr; k_ptr= k_ptr->ka_next)
	{
		if (!memcmp(&k_ptr->ka_cap, cap, sizeof(*cap)))
		{
			break;
		}
	}
	if (k_ptr)
	{
#if DEBUG
 { where(); printf("capability already in list\n"); endWhere(); }
#endif
		free(keepalive);
	}
	else
	{
		keepalive->ka_next= keepalive_list;
		keepalive_list= keepalive;
		sema_up(&keepalive_semaphore);
	}
#if DEBUG & 2
 { where(); printf("doing mu_unlock on keepalive_mutex\n"); endWhere(); }
#endif
	mu_unlock(&keepalive_mutex);
	return STD_OK;
}

errstat tcpip_unkeepalive_cap(cap)
capability *cap;
{
	keepalive_t *k_ptr, *k_prev;
	port cap_port;
	objnum cap_obj;

#if DEBUG
 { where(); printf("tcpip_unkeepalive_cap called with 0x%x\n", cap); endWhere(); }
#endif
#if DEBUG
 { where(); printf("doing mu_lock on keepalive_mutex\n"); endWhere(); }
#endif
	mu_lock(&keepalive_mutex);
	cap_port= cap->cap_port;
	cap_obj= prv_number(&cap->cap_priv);

	for (k_ptr= keepalive_list, k_prev= 0; k_ptr;
		k_prev= k_ptr, k_ptr= k_ptr->ka_next)
	{
		if (!memcmp(&k_ptr->ka_cap.cap_port, &cap_port, 
			sizeof(cap_port)) &&
			prv_number(&k_ptr->ka_cap.cap_priv) == cap_obj)
		{
			break;
		}
	}
	if (k_ptr)
	{
#if DEBUG
 { where(); printf("freeing cap\n"); endWhere(); }
#endif
		if (k_prev)
			k_prev->ka_next= k_ptr->ka_next;
		else
			keepalive_list= k_ptr->ka_next;
		free(k_ptr);
	}
	else
	{
#if DEBUG
 { where(); printf("cap not in list\n"); endWhere(); }
#endif
	}
#if DEBUG
 { where(); printf("doing mu_unlock on keepalive_mutex\n"); endWhere(); }
#endif
	mu_unlock(&keepalive_mutex);
	return STD_OK;
}

static void keepalive_thread(argp, args)
char *argp;
int args;
{
	keepalive_t *k_ptr, **ptr_ptr;
	long curr_tim, try_time;
	int result, respite;

assert (!argp && !args);

	for (;;)
	{
#if DEBUG & 2
 { where(); printf("doing mu_lock on keepalive_mutex\n"); endWhere(); }
#endif
		mu_lock(&keepalive_mutex);
		if (!keepalive_list)
		{
#if DEBUG
 { where(); printf("doing mu_unlock on keepalive_mutex\n"); endWhere(); }
#endif
			mu_unlock(&keepalive_mutex);
			sema_down(&keepalive_semaphore);
			continue;
		}
		curr_tim= sys_milli();
		k_ptr= keepalive_list;
		if (k_ptr->ka_time-curr_tim < 0)
		{
#if DEBUG & 2
 { where(); printf("doing mu_unlock on keepalive_mutex\n"); endWhere(); }
#endif
			mu_unlock(&keepalive_mutex);
			result= tcpip_keepalive(&k_ptr->ka_cap, &respite);
#if DEBUG & 2
 { where(); printf("doing mu_lock on keepalive_mutex\n"); endWhere(); }
#endif
			mu_lock(&keepalive_mutex);
#if DEBUG & 2
 { where(); printf("doing mu_lock on keepalive_mutex succeeded\n"); endWhere(); }
#endif
			curr_tim= sys_milli();

			for (ptr_ptr= &keepalive_list; *ptr_ptr &&
				*ptr_ptr != k_ptr; ptr_ptr=
				&(*ptr_ptr)->ka_next);
assert (*ptr_ptr);
			*ptr_ptr= k_ptr->ka_next;

			if (result != STD_OK)
			{
#if DEBUG
 { where(); printf("can't tcpip_keepalive: %s\n", err_why(result)); endWhere(); }
#endif
				free(k_ptr);
			}
			else
			{
#if DEBUG & 2
 { where(); printf("respite= %d\n", respite); endWhere(); }
#endif
				k_ptr->ka_time= curr_tim+(respite>>1);
				for (ptr_ptr= &keepalive_list; *ptr_ptr &&
					(*ptr_ptr)->ka_time <= k_ptr->ka_time; 
					ptr_ptr= &(*ptr_ptr)->ka_next);
				k_ptr->ka_next= *ptr_ptr;
				*ptr_ptr= k_ptr;
			}
		}
		if (!keepalive_list)
		{
#if DEBUG
 { where(); printf("doing mu_unlock on keepalive_mutex\n"); endWhere(); }
#endif
			mu_unlock(&keepalive_mutex);
			sema_down(&keepalive_semaphore);
			continue;
		}
		if (keepalive_list->ka_time-curr_tim <= 0)
		{
#if DEBUG & 2
 { where(); printf("doing mu_unlock on keepalive_mutex\n"); endWhere(); }
#endif
			mu_unlock(&keepalive_mutex);
			continue;
		}
		try_time= keepalive_list->ka_time-curr_tim;
assert (try_time>0);
#if DEBUG & 2
 { where(); printf("doing mu_unlock on keepalive_mutex\n"); endWhere(); }
#endif
		mu_unlock(&keepalive_mutex);
		sema_trydown(&keepalive_semaphore, try_time);
	}
}
