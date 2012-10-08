/*	@(#)flgrp_malloc.h	1.1	96/02/27 14:01:47 */
#ifndef __FLIP_GRP_MALLOC__
#define __FLIP_GRP_MALLOC__

/*
 * A memory allocation module (based on the original Amoeba malloc) that can
 * serve as a backend for other malloc modules.
 */

#include <malloctune.h>

typedef unsigned int	Blk_size;
typedef	struct Seg_hdr	Seg_hdr;
typedef struct Blk_hdr	Blk_hdr;

struct Seg_hdr
{
    Seg_hdr *	s_next;
    Seg_hdr *	s_prev;
    Blk_size	s_size;		/* size of segment (INcluding this header) */
};

struct Blk_hdr
{
    Blk_hdr *	b_free_next;
    Blk_hdr *	b_free_prev;
    Blk_hdr *	b_phys_next;
    Blk_hdr *	b_phys_prev;
    Blk_hdr **	b_class;
    Blk_size	b_size;
};

struct Mem_descr {
    int		mal_valid;	/* Set to one when Mem_descr is initialized */

    /* The routine called when more memory is needed */
    char *	(*mal_getblk) _ARGS((struct Mem_descr *_desc, vir_bytes _siz));

    /* The routine called when the memory is freed again */
    void	(*mal_putblk) _ARGS((struct Mem_descr *_desc, char *_seg));

    /* Smallest segment size supported by mal_getblk() */
    Blk_size	mal_segsize;

    /* The head of list of allocated segments */
    Seg_hdr *   mal_seglist;

    /* The free list heads for each class */
    Blk_hdr *	mal_freelist[NUM_CLASSES];
};

typedef struct Mem_descr  Mem_descr;

#include <_ARGS.h>
#include <stdlib.h>

void mal_init_descr   _ARGS((Mem_descr * _m, char * (*_getblk)(),
			   void	(*_putblk)(), Blk_size _minsize));
void mal_delete       _ARGS((Mem_descr *));

_VOIDSTAR mal_malloc  _ARGS((Mem_descr * _m, size_t _nbytes));
_VOIDSTAR mal_calloc  _ARGS((Mem_descr * _m, size_t _num, size_t _elmsize));
void      mal_free    _ARGS((Mem_descr * _m, _VOIDSTAR _addr));
_VOIDSTAR mal_realloc _ARGS((Mem_descr * _m, _VOIDSTAR addr, size_t _newsize));

#endif /* __FLIP_GRP_MALLOC__ */
