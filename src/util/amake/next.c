/*	@(#)next.c	1.2	94/04/07 14:52:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * Define and initialize structure free lists.
 */

struct Generic *h_Generic = 0;
struct attr_descr *h_attr_descr = 0;
struct attr_value *h_attr_value = 0;
struct binding *h_binding = 0;
struct cached *h_cached = 0;
struct cluster *h_cluster = 0;
struct cons *h_cons = 0;
struct derivation *h_derivation = 0;
struct dj *h_dj = 0;
struct function *h_function = 0;
struct graph *h_graph = 0;
struct invocation *h_invocation = 0;
struct job *h_job = 0;
struct objclass *h_objclass = 0;
struct object *h_object = 0;
struct param *h_param = 0;
struct process *h_process = 0;
struct scope *h_scope = 0;
struct slist *h_slist = 0;
struct tool *h_tool = 0;
struct toolinstance *h_toolinstance = 0;
struct type *h_type = 0;
struct variable *h_variable = 0;

#ifdef DEBUG
int cnt_Generic = 0;
int cnt_attr_descr = 0;
int cnt_attr_value = 0;
int cnt_binding = 0;
int cnt_cached = 0;
int cnt_cluster = 0;
int cnt_cons = 0;
int cnt_derivation = 0;
int cnt_dj = 0;
int cnt_function = 0;
int cnt_graph = 0;
int cnt_invocation = 0;
int cnt_job = 0;
int cnt_objclass = 0;
int cnt_object = 0;
int cnt_param = 0;
int cnt_process = 0;
int cnt_scope = 0;
int cnt_slist = 0;
int cnt_tool = 0;
int cnt_toolinstance = 0;
int cnt_type = 0;
int cnt_variable = 0;
#endif
