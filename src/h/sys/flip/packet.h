/*	@(#)packet.h	1.7	96/02/27 10:40:10 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef _SYS_FLIP_PACKET_H_
#define _SYS_FLIP_PACKET_H_

#include <string.h>

/*
 * Packet structure and protocol stacking macros
 */

#define PKTENDHDR	100	/* Point from where headers are prepended */
#define PKTBEGHDR	42	/* End of link header at input */

typedef struct packet pkt_t, *pkt_p;
typedef struct packetpool pool_t, *pool_p;

void pkt_init _ARGS(( pool_p pool, int size, pkt_p bufs, int nbufs, char *data,
	void (*notify) _ARGS(( long arg, pkt_p pkt )), long arg ));
void pkt_discard _ARGS(( pkt_p pkt ));
pkt_p pkt_new _ARGS(( pkt_p pkt ));
pkt_p pkt_acquire _ARGS(( void ));
int pkt_copy _ARGS(( pkt_p pkt1, pkt_p pkt2 ));
int pkt_pullup _ARGS(( pkt_p pkt1, int size ));
int pkt_setwanted _ARGS(( pool_p pool, int n ));
int pkt_incwanted _ARGS(( pool_p pool, int n ));

typedef struct header_admin {
	char 	*ha_curheader;		/* Actually struct unknown * */
	int	ha_size;		/* Size of "active header" */
	int	ha_type;		/* Where is header ? */
#define HAT_ABSENT	0
#define HAT_INPLACE	1
#define HAT_OUTTHERE	2
} ha_t;

struct packet_admin {
	pkt_p	pa_next;
	pool_p	pa_pool;
	int	pa_size;		/* possible direct data */
	void	(*pa_release) _ARGS(( long arg ));
					/* release procedure */
	long	pa_arg;			/* arg to pa_release */
	int    *pa_wait;		/* block until all are packets gone? */
	int	pa_allocated;		/* packet allocated? */
	int	pa_priority;		/* priority value */
	ha_t	pa_header;
};

struct packet_contents {
	int	pc_dirsize;		/* Size of data in pc_buffer */
	int	pc_offset;		/* Pointer to start of data */
	int	pc_totsize;		/* Total size, including "user" data */
	int	pc_dstype;		/* how to get user buffer */
	int	pc_dsident;		/* ditto */
	long	pc_virtual;		/* ditto */
	char	*pc_buffer;		/* Direct data store */
};

struct packet {
	struct packet_admin	p_admin;
	struct packet_contents	p_contents;
};

/*
 **************************************************************************
 * protocol stacking and unstacking code
 */

/*
 * First a trick to get around C's lack of alignof operator
 * I hope it ports to any compiler reasonable for us to use.
 */
#define alignof(tipe) (sizeof(struct { char initial; tipe contents;}) - \
			sizeof(tipe))

#define aligned(p, align) ((((long) (p)) & ((align)-1)) == 0)

#define pkt_offset(pkt)	\
	(&(pkt)->p_contents.pc_buffer[(pkt)->p_contents.pc_offset])

#define pkt_set_release(pkt, func, arg) \
	do { \
	    assert((pkt)->p_admin.pa_release == 0); \
	    (pkt)->p_admin.pa_release = func; \
	    (pkt)->p_admin.pa_arg = arg; \
	} while (0)

/*
 * When you are going to build a packet get it by calling proto_init
 */
#define macro_proto_init(pkt) \
    do { \
	assert((pkt)->p_admin.pa_allocated); \
	(pkt)->p_admin.pa_header.ha_type = HAT_ABSENT; \
	(pkt)->p_contents.pc_dirsize = 0; \
	(pkt)->p_contents.pc_offset = PKTENDHDR; \
	(pkt)->p_contents.pc_totsize = 0; \
	(pkt)->p_contents.pc_virtual = 0; \
    } while (0)

#ifdef ROMKERNEL
void func_proto_init _ARGS(( pkt_p pkt ));
#define proto_init(pkt) func_proto_init(pkt)
#else
#define proto_init(pkt) macro_proto_init(pkt)
#endif


/*
 * Total size of packet
 */
#define	pkt_size(pkt)	((pkt)->p_contents.pc_totsize)


/*
 * The user data part of the packet is then set by
 */
#define proto_set_virtual(pkt, dstype, dsident, virtual, size) \
    do { \
	assert((pkt)->p_admin.pa_allocated); \
	assert((pkt)->p_contents.pc_totsize==(pkt)->p_contents.pc_dirsize); \
	(pkt)->p_contents.pc_dstype = dstype; \
	(pkt)->p_contents.pc_dsident = dsident; \
	(pkt)->p_contents.pc_virtual = virtual; \
	(pkt)->p_contents.pc_totsize += size; \
    } while(0)


/*
 * Direct data added by
 */

#define proto_dir_append(pkt, buf, size) \
    do { \
	assertN(1,(pkt)->p_admin.pa_allocated); \
	assertN(2,(pkt)->p_contents.pc_offset + (pkt)->p_contents.pc_dirsize + \
		size <= (pkt)->p_admin.pa_size); \
	(void) memmove( \
		(_VOIDSTAR)(pkt_offset(pkt) + (pkt)->p_contents.pc_dirsize), \
		(_VOIDSTAR) buf, (size_t) size); \
	(pkt)->p_contents.pc_dirsize += size; \
	(pkt)->p_contents.pc_totsize += size; \
    } while (0)

#define proto_dir_getoff(pkt, buf, size) \
    do { \
	assert((pkt)->p_admin.pa_allocated); \
	assert((pkt)->p_contents.pc_dirsize >= size); \
	(pkt)->p_contents.pc_dirsize -= size; \
	(pkt)->p_contents.pc_totsize -= size; \
	(void) memmove((_VOIDSTAR) buf, \
		(_VOIDSTAR) (pkt_offset(pkt) + (pkt)->p_contents.pc_dirsize), \
							    (size_t) size); \
    } while (0)

/*
 * and headers added and fixed in place by
 */

char *func_proto_add_header _ARGS(( pkt_p pkt, unsigned hsize,
					unsigned halign ));

#define	proto_add_header(pkt, tipe) \
	(tipe *) func_proto_add_header(pkt, sizeof(tipe), alignof(tipe))

#ifdef ROMKERNEL
#define PROTO_ADD_HEADER(hdr, pkt, tipe) \
	hdr = (tipe *) func_proto_add_header(pkt, sizeof(tipe), alignof(tipe))
#else /* ROMKERNEL */
/* This macro does not align headers. */
#define PROTO_ADD_HEADER(hdr, pkt, tipe) \
	do { \
		assert((pkt)->p_admin.pa_allocated); \
		assert((pkt)->p_admin.pa_header.ha_type == HAT_ABSENT); \
		(pkt)->p_contents.pc_totsize += sizeof(tipe); \
		(pkt)->p_contents.pc_dirsize += sizeof(tipe); \
		(pkt)->p_contents.pc_offset -= sizeof(tipe); \
		assert((pkt)->p_contents.pc_offset >= 0); \
		(pkt)->p_admin.pa_header.ha_size = sizeof(tipe); \
        	assert(aligned(pkt_offset(pkt), alignof(tipe))); \
		(pkt)->p_admin.pa_header.ha_type = HAT_INPLACE; \
		(pkt)->p_admin.pa_header.ha_curheader = \
							pkt_offset(pkt);\
		(hdr) = (tipe *)  (pkt)->p_admin.pa_header.ha_curheader; \
	} while(0)
#endif /* ROMKERNEL */

#define macro_proto_fix_header(pkt) \
    do { \
	assert((pkt)->p_admin.pa_allocated); \
	assert((pkt)->p_admin.pa_header.ha_type != HAT_ABSENT); \
	if ((pkt)->p_admin.pa_header.ha_type == HAT_OUTTHERE) \
		(void)memmove((_VOIDSTAR) pkt_offset(pkt), \
			(_VOIDSTAR) (pkt)->p_contents.pc_buffer, \
			(size_t) (pkt)->p_admin.pa_header.ha_size); \
	(pkt)->p_admin.pa_header.ha_type = HAT_ABSENT; \
    } while (0)

#ifdef ROMKERNEL
void func_proto_fix_header _ARGS(( pkt_p pkt ));
#define proto_fix_header(pkt) func_proto_fix_header(pkt)
#else
#define proto_fix_header(pkt) macro_proto_fix_header(pkt)
#endif

/*
 * Taking one off:
 */

char *func_proto_look_header _ARGS(( pkt_p pkt, unsigned hsize,
					unsigned halign ));

#define proto_look_header(pkt, tipe) \
	(tipe *) func_proto_look_header(pkt, sizeof(tipe), alignof(tipe))

#ifdef ROMKERNEL
#define PROTO_LOOK_HEADER(hdr, pkt, tipe) \
	hdr = (tipe *) func_proto_look_header(pkt, sizeof(tipe), alignof(tipe))
#else /* ROMKERNEL */
/* The macro version does not align headers. */
#define PROTO_LOOK_HEADER(hdr, pkt, tipe) \
	do { \
        	assertN(1, (pkt)->p_admin.pa_allocated); \
        	assertN(2, (pkt)->p_admin.pa_header.ha_type == HAT_ABSENT); \
        	if ((pkt)->p_contents.pc_totsize < sizeof(tipe)) { \
                	(hdr) = 0;       /* Packet is wrong */ \
			break; \
		} \
		assertN(3, (pkt)->p_contents.pc_dirsize >= sizeof(tipe)); \
        	assertN(4, aligned(pkt_offset(pkt), alignof(tipe))); \
        	(pkt)->p_admin.pa_header.ha_size = sizeof(tipe); \
		(pkt)->p_admin.pa_header.ha_type = HAT_INPLACE; \
               	(pkt)->p_admin.pa_header.ha_curheader = pkt_offset(pkt); \
        	hdr = (tipe *) (pkt)->p_admin.pa_header.ha_curheader; \
	} while(0)
#endif /* ROMKERNEL */


#define proto_cur_header(hdr, pkt, tipe) \
		assert((pkt)->p_admin.pa_header.ha_type != HAT_ABSENT); \
		hdr = (tipe *) (pkt)->p_admin.pa_header.ha_curheader;

#define proto_cur_size(pkt)	((pkt)->p_contents.pc_dirsize)

#define macro_proto_remove_header(pkt) \
    do { \
	assert((pkt)->p_admin.pa_allocated); \
	assert((pkt)->p_admin.pa_header.ha_type != HAT_ABSENT); \
	(pkt)->p_contents.pc_totsize -= (pkt)->p_admin.pa_header.ha_size; \
	(pkt)->p_contents.pc_dirsize -= (pkt)->p_admin.pa_header.ha_size; \
	(pkt)->p_contents.pc_offset += (pkt)->p_admin.pa_header.ha_size; \
	(pkt)->p_admin.pa_header.ha_type = HAT_ABSENT; \
    } while(0)

#ifdef ROMKERNEL
void func_proto_remove_header _ARGS(( pkt_p pkt ));
#define proto_remove_header(pkt) func_proto_remove_header(pkt)
#else
#define proto_remove_header(pkt) macro_proto_remove_header(pkt)
#endif

#define proto_setup_input(pkt, tipe) \
    do { \
	assert((pkt)->p_admin.pa_allocated); \
	(pkt)->p_admin.pa_header.ha_type = HAT_ABSENT; \
	(pkt)->p_contents.pc_virtual = 0; \
	(pkt)->p_contents.pc_offset = PKTBEGHDR - sizeof(tipe); \
	(pkt)->p_contents.pc_totsize = 0; \
	(pkt)->p_contents.pc_dirsize = (pkt)->p_admin.pa_size-PKTBEGHDR+ \
							sizeof(tipe); \
    } while (0)

#define proto_set_dir_size(pkt, size) \
    do { \
	(pkt)->p_contents.pc_dirsize= size; \
    } while(0)
	

/*
 **************************************************************************
 * definition of pool of packets.
 *
 * Each hard or software interface has one, and packets always return to their
 * own pool. Touching isn't it?
 */

struct packetpool {
	pool_p	pp_next;		/* Pointer to next pool, debugging */
	int	pp_maxbuf;		/* maximum in this pool */
	int	pp_nbuf;		/* current number in this pool */
	pkt_p	pp_freelist;		/* list of available packets */
	int	pp_flags;		/* Who knows ? */
	int 	pp_wanted;		/* somebody needs packets */
	void	(*pp_notify) _ARGS(( long arg, pkt_p pkt));	
					/* Routine to call */
	long	pp_arg;			/* Argument for it */
};

#define MACRO_PKT_GET(pkt, pool)	\
	do { \
		if ((pool)->pp_freelist==0) { \
			(pkt) = 0; \
			break; \
		} \
		(pkt) = (pool)->pp_freelist; \
		(pool)->pp_freelist = (pkt)->p_admin.pa_next; \
		(pool)->pp_nbuf--; \
		assert((pool)->pp_nbuf >= 0); \
		assert(!(pkt)->p_admin.pa_allocated); \
		(pkt)->p_admin.pa_next = 0; \
		(pkt)->p_admin.pa_allocated = 1; \
	} while(0)

#ifdef ROMKERNEL
pkt_p func_pkt_get _ARGS(( pool_p pool ));
#define PKT_GET(pkt, pool) pkt = func_pkt_get(pool)
#else
#define PKT_GET(pkt, pool) MACRO_PKT_GET(pkt, pool)
#endif

#define pkt_wanted(pool, n)	\
	do { \
		register pkt_p pkt;\
		(pool)->pp_wanted += (n); \
		assert((pool)->pp_notify != 0); \
		while ((pool)->pp_wanted > 0 && (pool)->pp_freelist != 0) { \
			PKT_GET(pkt, pool); \
			(pool)->pp_wanted--; \
			assert((pool)->pp_wanted >= 0); \
			(*(pool)->pp_notify)((pool)->pp_arg, pkt); \
		} \
	} while(0)

#endif /* _SYS_FLIP_PACKET_H_ */
