/*	@(#)cluster.h	1.2	94/04/07 14:47:59 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */


EXTERN struct cluster *CurrentCluster;
EXTERN struct idf *Id_source, *Id_target;

/* a running cluster may be in one of the following states: */
enum ClState {
	CL_SRC, 	/* computing sources			    */
	CL_TAR, 	/* computing targets			    */
	CL_SYN, 	/* synchronizing with other clusters	    */
	CL_CHK, 	/* look whether this cluster should be done */
	CL_CMD, 	/* determining and starting up tools	    */
	CL_COMPUTE,	/* handle hanging mkdep-like programs	    */
	CL_WAIT		/* wait for the tools to finish		    */
};

struct binding {
    struct job	      *b_compute;	/* 'mkdep'-like job */
    struct invocation *b_inv;		/* 'cc-c'-like invocation */
};

/* allocation definitions of struct binding */
#include "structdef.h"
DEF_STRUCT(struct binding, h_binding, cnt_binding)
#define new_binding()     NEW_STRUCT(struct binding, h_binding, cnt_binding, 5)
#define free_binding(p)   FREE_STRUCT(struct binding, h_binding, p)


struct cluster {
    struct expr    *cl_ident;		/* cluster name expression, or NULL  */
    struct idf	   *cl_idp;		/* cluster identifier		     */
    int		    cl_flags;
    struct scope   *cl_scope;           /* cluster parameters/defaults       */
    struct expr	   *cl_targets;
    struct expr    *cl_orig_targets;    /* targets we start with 	     */
    struct expr	   *cl_sources;	
    struct job     *cl_job;		/* job handling this cluster 	     */
    enum ClState    cl_state;		/* cluster `program counter'	     */
    union {
    struct slist   *cl__invocations;	/* may change the defaults 	     */
    struct expr    *cl__command;	/* imperative command		     */
    } cl_update;
    struct slist   *cl_tool_list;
    struct graph   *cl_graph;		/* for the new `class' algorithm     */
    struct slist   *cl_persistent;	/* should be re-used in cluster      */
    struct slist   *cl_def_inputs;	/* list of (tool => def.comp.inputs) */
    struct slist   *cl_calls;           /* invocation expressions/results    */
} *new_cluster P(( void ));

#define cl_invocations	cl_update.cl__invocations
#define cl_command	cl_update.cl__command


/* allocation definitions of struct cluster */
DEF_STRUCT(struct cluster, h_cluster, cnt_cluster)
#define new_cluster()     NEW_STRUCT(struct cluster, h_cluster, cnt_cluster, 5)
#define free_cluster(p)   FREE_STRUCT(struct cluster, h_cluster, p)


EXTERN struct idf *Id_persistent;
/* Used to indicate that the object should not be removed from the sources
 * after being used as (explicit) input.
 */

/* possible cl_flags: */
#define C_ANONYMOUS	0x01 /* cluster with no name given */
#define C_IMPERATIVE	0x02 /* cluster with DO clause */
#define C_VISITED	0x04 /* used in determining which clusters to be run */
#define C_POTENTIAL	0x08 /* cluster might need to be run */
#define C_DO_RUN	0x10

void AllClustersKnown	P((void));
void EvalDefault	P((struct expr *call , struct expr *arrow ));
void ClusterAlgorithm	P((struct cluster **clusp ));
void EnterCluster	P((struct cluster *clus ));
void InitClusters	P((struct slist *to_do ));
void SolveSequenceProblems
			P((struct invocation *inv , int scope ));
void DetermineRelevantAttrValues
			P((struct slist *objects , struct slist *attr_list ));
int  IsSource		P((struct expr *obj ));
int  IsTarget		P((struct expr *obj ));

