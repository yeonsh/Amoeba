/*	@(#)classes.h	1.3	94/04/07 14:47:44 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

typedef struct slist *class_list;

struct graph {
    struct slist *gr_tools;
    class_list    gr_classes;
};

void DoPrintGraph P((struct graph *graph ));
void PutInvocationInGraph P((struct graph *graph , struct expr *inv , int index ));
void DoAttributes P((struct tool *tool ));
void DeclareTool P((struct tool *tool ));
void InitClasses P((void));
void PutObjectsInGraph P((struct graph *graph , struct slist *objects ));
void InitGraph P((struct graph **graphp ));
int PruneGraph P((struct graph *graph , struct cluster *clus ));
void ChangeToolOrder P((struct cluster *clus ));

#ifdef DEBUG
#define PrintGraph(graph)	DoPrintGraph(graph)
#else
#define PrintGraph(graph)	{ /* do nothing */ }
#endif

