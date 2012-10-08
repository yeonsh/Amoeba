/*	@(#)block.c	1.8	96/02/27 13:15:17 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* STARCH - STore and reSTore ARCHives of graphs.
   module implementing reading and writing archive blocks.
	Author: Ed Keizer, 1995
*/

#include "starch.h"
#include "block.h"
#ifdef	MULTI_THREADED
#include <thread.h>
#endif
#include "io.h"
#include <module/tape.h>
#include <module/disk.h>
#ifdef __STDC__
#include <time.h>
#else
#include <posix/time.h>
#endif

extern int	do_list ;

#ifdef MULTI_THREADED

/* Indicates whether to go through buffer pool */
int		r_single_threaded ;
int		w_single_threaded ;
static int	writer_running ;

int		niobuf = N_IOBUFS ;	/* # of io buffers */

struct pool	freepool ;		/* Blocks for grab */
struct pool	readpool ;		/* Blocks read */
struct pool	writepool ;		/* Blocks to be written */
int		free_init ;		/* freepool was inited */

#ifdef __STDC__
static void	initpool(struct pool *);
static void	fillpool(struct pool *,int size, int n) ;
static void	read_thread(char *p, int s);
static void	write_thread(char *p, int s);
static void	wio_flushed(struct pool *);
#else
static void	initpool();
static void	fillpool() ;
static void	read_thread();
static void	write_thread();
static void	wio_flushed();
#endif

#endif

struct ba_list	*w_io ;			/* Current Write Buffer */
struct ba_list	*r_io ;			/* Current Read Buffer */

/* Read no further, EOF seen on input */
int		eof_seen ;

#ifdef __STDC__
static void	wbl_zero(void);
static void	snd_wbl(void);
#else
static void	wbl_zero();
static void	snd_wbl();
#endif

void
#ifdef __STDC__
init_write(void)
#else
init_write()
#endif
{
	static int	only_once ;

	init_wio() ;

	/* The next sections needs to be done only once */
	if ( only_once ) return ;
	only_once=1 ;

#ifdef MULTI_THREADED
	/* Initialize buffer admin */
	initpool(&writepool) ;
	if ( !free_init ) {
		initpool(&freepool) ;
		free_init=1 ;
	}
	if ( niobuf<1 ) {
		if ( niobuf<0 ) warning("# of buffers (-n) should be >=1") ;
		niobuf=1 ;
	}
	fillpool(&freepool, BLOCK_SIZE, niobuf );
	if ( niobuf>1 ) {
		if ( thread_newthread(write_thread,STK_SIZE,(char *)0,0)==0 ) {
			warning("can not create write thread, running single threaded");
			w_single_threaded=1 ;
		}
	} else {
		w_single_threaded=1 ;
	}
#else
	w_io= (struct ba_list *)getmem(sizeof (struct ba_list));
	w_io->content= getmem(BLOCK_SIZE) ;
#endif

}

#ifdef MULTI_THREADED
static void
#ifdef __STDC__
initpool(struct pool *thispool)
#else
initpool(thispool)
	struct pool	*thispool;
#endif
{
	/* Initialize a pool, assume that we are alone */
	sema_init(&thispool->count,0) ;
	thispool->last= (struct ba_list *)0 ;
	thispool->first= (struct ba_list *)0 ;
	thispool->s_total= 0 ;
	thispool->s_no= 0 ;
	thispool->s_max= 0 ;
	/* Initialize access lock */
	mu_init(&thispool->lock) ;
	/* Initialise finished lock to locked, unlock when finished */
	mu_init(&thispool->finished) ;
	mu_lock(&thispool->finished) ;
}


static void   
#ifdef __STDC__
fillpool(struct pool *thispool, int size, int n)
#else
fillpool(thispool,size,n)
        struct pool	*thispool;
	int		size ;		/* Size of individual buffers */
	int		n ;		/* Number of buffers */
#endif
{
	int		i ;
	struct ba_list	*allocated ;
	byte		*buffers ;

	allocated= (struct ba_list *)getmem(n*sizeof (struct ba_list));
	buffers= (byte *)getmem(n*size) ;
	for ( i=0 ; i<n ; i++ ) {
		allocated[i].content= buffers + i*size ;
		(void)put_block(thispool, allocated+i, AT_BEGIN) ;
	}
}

void
pool_abort(thispool,get_flag)
        struct pool     *thispool;
	int get_flag ;
{
	struct ba_list	*item ;
	struct ba_list	*next ;

	mu_lock(&thispool->lock);
	if ( get_flag ) {
		thispool->get_aborted=1 ;
	} else {
		thispool->put_aborted=1 ;
		wio_flushed(thispool) ;
	}
	/* Flush all existing buffers to the free pool */
	if ( thispool== &freepool ) {
		warning("internal error: aborting free buffer pool");
	} else {
		/* we have to do this to avoid that the main thread hangs
		   in a get_block from the free pool
		 */
		for ( item=thispool->first ; item ; item= next ) {
			next= item->next ;
			item->flags=0 ;
			put_block(&freepool,item,AT_END);
		}
		thispool->first= thispool->last= (struct ba_list *)0 ;
	}
		
	mu_unlock(&thispool->lock);
}

int
#ifdef __STDC__
put_block(struct pool *thispool, struct ba_list *item, int pos)
#else
put_block(thispool,item,pos)
        struct pool	*thispool;
	struct ba_list	*item ;
	int		pos ; /* AT_BEGIN or AT_END */
#endif
{
	int		ql ;

	mu_lock(&thispool->lock);
	if ( thispool->put_aborted ) {
		mu_unlock(&thispool->lock);
		return EXCEPTION ;
	}
	if ( item->flags&BA_ABORT ) {
		thispool->get_aborted=1 ;
	}
	if ( pos==AT_BEGIN ) {
		item->next= thispool->first ;
		if ( !thispool->first ) thispool->last=item ;
		thispool->first= item ;
	} else {/*AT_END*/
		item->next= (struct ba_list *)0;
		if ( !thispool->first ) {
			thispool->first= item ;
		} else {
			thispool->last->next= item ;
		}
		thispool->last= item ;
	}
	ql=sema_level(&thispool->count) ;
	thispool->s_total += ql ;
	thispool->s_no++ ;
	if ( ql>thispool->s_max ) thispool->s_max = ql ;
	mu_unlock(&thispool->lock);
	sema_up(&thispool->count);
	return OK ;
}

struct ba_list *
#ifdef __STDC__
get_block(struct pool *thispool)
#else
get_block(thispool)
        struct pool	*thispool;
#endif
{
	struct ba_list	*item ;

	sema_down(&thispool->count) ;
	mu_lock(&thispool->lock) ;
	item= thispool->first ;
	if ( !item ) fatal("unexpected empty pool list");
	thispool->first= item->next ;
	if ( !thispool->first ) thispool->last= (struct ba_list *)0;
	if ( thispool->get_aborted ) item->flags= BA_ABORT ;
	mu_unlock(&thispool->lock);
	return item ;
}

static void
write_thread(p,s) char *p ; int s ; {
	struct ba_list	*my_buffer ;

	writer_running++ ;
	for(;;) {
		my_buffer= get_block(&writepool) ;
		if ( out_block(my_buffer) == EXCEPTION ) {
			pool_abort(&writepool,0);
			break ;
		}
		if ( my_buffer->flags&BA_END ) wio_flushed(&writepool) ;
		(void)put_block(&freepool,my_buffer,AT_END) ;
	}
}

static void
#ifdef __STDC__
wio_flushed(struct pool *thispool)
#else
wio_flushed(thispool)
	struct pool	*thispool ;
#endif
{
	mu_unlock(&thispool->finished) ;
}

#endif /* MULTI_THREADED */


#ifdef __STDC__
void	w_start(void)
#else
void	w_start()
#endif
{
	if ( !w_io ) {
#ifdef MULTI_THREADED
		w_io= get_block(&freepool) ;
#else
		fatal("internal: w_start single thread problem");
#endif
	}
	w_io->rw_pos=0 ;
	wbl_seek(8) ; /* Skip volno & seqno */
	wbl_4b((long)HDR_CONSTANT);
	if ( CHK_OFF!=wbl_tell() ) {
		fatal("internal inconsistency in checksum location");
	}
	wbl_2b(0); /* To be filled in */
}


#ifdef __STDC__
void	w_end(void)
#else
void	w_end()
#endif
{
	if ( wbl_tell()==CHK_OFF+2 ) return ;
		/* Relies on re-executability of w_start() */
	wbl_zero();
	snd_wbl();
	return ;
}

#ifdef __STDC__
void	aw_hdr(int type)
#else
void	aw_hdr(type)
	int	type ;
#endif
{
	if ( !w_io ) {
#ifdef MULTI_THREADED
		w_io= get_block(&freepool) ;
#else
		fatal("internal: w_start single thread problem");
#endif
	}
	aio_hdr(w_io,type);
	w_end() ;
}

/* Get a block and initialize it */

/* Copy of w_start, but with adresses and external buffer admin */
void
#ifdef __STDC__
wio_start(struct ba_list *p_io)
#else
wio_start(p_io)
	struct ba_list	*p_io ;
#endif
{
	/* Skip volno & seqno */
	p_io->rw_pos=8 ;
	/* Write constant */
	wio_4b(p_io,(long)HDR_CONSTANT) ;
	/* Skip checksum */
	p_io->rw_pos+= 2 ;
}


/* Copy of w_end, but with adresses and external buffer admin */
#ifdef __STDC__
void	wio_end(struct ba_list *p_io)
#else
void	wio_end(p_io)
		struct ba_list *p_io;
#endif
{
	register byte	*dst ;
	register byte	*last ;

	last= p_io->content+BLOCK_SIZE ;
	for ( dst=p_io->content+p_io->rw_pos ; dst<last ; ) {
		*dst++= 0 ;
	}
	p_io->rw_pos= BLOCK_SIZE ;
}

/* snd_wbl sees whether spool buffer is used, if not go straight to out_block,
   if so, let buffer stuff call out_block.
 */

static
#ifdef __STDC__
void snd_wbl(void)
#else
void snd_wbl()
#endif
{
	w_io->flags=0 ;	
#ifdef MULTI_THREADED
	if ( !w_single_threaded ) {
		if ( put_block(&writepool,w_io,AT_END) == EXCEPTION ) {
			t_finish();
		}
		/* And invalidate this for w_start */
		w_io= (struct ba_list *)0;
	} else
#endif
	{
		if ( out_block(w_io) == EXCEPTION ) t_finish();
	}
}


void
#ifdef __STDC__
ar_flush(void)
#else
ar_flush()
#endif
{
	int	result ;

#ifdef MULTI_THREADED
	if ( !w_io ) w_io=get_block(&freepool);
#endif
	w_io->flags=BA_END ;
#ifdef MULTI_THREADED
	if ( w_single_threaded ) {
		result= out_block(w_io) ;
		(void)put_block(&freepool,w_io,AT_END);
	} else {
		put_block(&writepool,w_io,AT_END) ;
		/* Wait for data to flush */
		mu_lock(&writepool.finished) ;
		result= writepool.put_aborted ? EXCEPTION: OK ;
	}
	/* And invalidate this for w_start */
	w_io= (struct ba_list *)0;
#else
	result= out_block(w_io) ;
	free((char *)w_io->content);
	free((char *)w_io);
#endif
	if ( result==EXCEPTION ) t_finish() ;
}

void
#ifdef __STDC__
wbl_string(char *s)
#else
wbl_string(s)
	char	*s ;
#endif
{
	register char	*src ;
	register byte	*dst ;

	src=s ; dst=w_io->content+w_io->rw_pos ;
	while ( *src && dst-w_io->content<BLOCK_SIZE-1 ) {
		*dst++= *src++ ;
	}
	if ( *src ) fatal("internal:string ran out of block") ;
	*dst++=0 ;
	w_io->rw_pos= dst-w_io->content ;
}

/* A copy of the above, but with adresses */
void
#ifdef __STDC__
wio_string(struct ba_list *p_io,char *s)
#else
wio_string(p_io,s)
	struct ba_list	*p_io ;
	char		*s ;
#endif
{
	register char	*src ;
	register byte	*dst ;
	register byte	*last ;

	src=s ; dst=p_io->content+p_io->rw_pos ;
	last=p_io->content+BLOCK_SIZE-1 ;
	while ( *src && dst<last ) {
		*dst++= *src++ ;
	}
	if ( *src ) fatal("internal:string ran out of block") ;
	*dst++=0 ;
	p_io->rw_pos= dst-p_io->content ;
	return ;
}

void
#ifdef __STDC__
wbl_mem(int n,byte *mem)
#else
wbl_mem(n,mem)
	int	n;
	byte	*mem ;
#endif
{
	register byte	*src ;
	register byte	*dst ;
	register int	cnt=n ;

	if ( w_io->rw_pos+n>BLOCK_SIZE ) fatal("internal:wbl_mem ran out of block") ;
	src=mem ; dst=w_io->content+w_io->rw_pos ;
	while ( cnt-- ) {
		*dst++= *src++ ;
	}
	w_io->rw_pos= dst-w_io->content ;
}


static void
#ifdef __STDC__
wbl_zero(void)
#else
wbl_zero()
#endif
{
	register byte	*dst ;

	dst=w_io->content+w_io->rw_pos ;
	while ( w_io->rw_pos<BLOCK_SIZE ) {
		*dst++= 0 ; w_io->rw_pos++ ;
	}
}

void
#ifdef __STDC__
wbl_cap(capability *cap)
#else
wbl_cap(cap)
	capability	*cap ;
#endif
{
	register byte	*src ;
	register byte	*dst ;
	register int	i = CAP_SZ ;

	if ( w_io->rw_pos>BLOCK_SIZE-CAP_SZ ) fatal("internal:wbl_cap ran out of block") ;
	src= (byte *) cap ;
	dst=w_io->content+w_io->rw_pos ;
	while ( i-- ) {
		*dst++= *src++ ;
	}
	w_io->rw_pos += CAP_SZ ;
}

void
#ifdef __STDC__
wbl_4b(long val)
#else
wbl_4b(val)
	long	val ;
#endif
{
	register byte	*dst ;
	register int	i = 4 ;

	if ( w_io->rw_pos>BLOCK_SIZE-4 ) fatal("internal:4b ran out of block") ;
	dst=w_io->content+w_io->rw_pos+i ;
	while ( i-- ) {
		*--dst= val & 0xFF ;
		val >>= 8 ;
	}
	w_io->rw_pos += 4 ;
}

void
#ifdef __STDC__
wbl_2b(unsigned val)
#else
wbl_2b(val)
	unsigned	val ;
#endif
{
	register byte	*dst ;
	register int	i = 2 ;

	if ( w_io->rw_pos>BLOCK_SIZE-2 ) fatal("internal:2b ran out of block") ;
	dst=w_io->content+w_io->rw_pos+i ;
	while ( i-- ) {
		*--dst= val & 0xFF ;
		val >>= 8 ;
	}
	w_io->rw_pos += 2 ;
}

/* Copies of the two above, but writes through buffer pointers */
void
#ifdef __STDC__
wio_4b(struct ba_list *p_io,long val)
#else
wio_4b(p_io,val)
	struct ba_list *p_io ;
	long	val ;
#endif
{
	register byte	*dst ;
	register int	i = 4 ;

	p_io->rw_pos += i ;
	dst=p_io->content+p_io->rw_pos ;
	if ( p_io->rw_pos>BLOCK_SIZE ) fatal("internal:4b ran out of block") ;
	while ( i-- ) {
		*--dst= val & 0xFF ;
		val >>= 8 ;
	}
}

void
#ifdef __STDC__
wio_2b(struct ba_list *p_io,unsigned val)
#else
wio_2b(p_io,val)
	struct ba_list *p_io ;
	unsigned	val ;
#endif
{
	register byte	*dst ;
	register int	i = 2 ;

	p_io->rw_pos += i ;
	dst=p_io->content+p_io->rw_pos ;
	if ( p_io->rw_pos>BLOCK_SIZE ) fatal("internal:2b ran out of block") ;
	while ( i-- ) {
		*--dst= val & 0xFF ;
		val >>= 8 ;
	}
}

void
#ifdef __STDC__
wbl_1b(int val)
#else
wbl_1b(val)
	int	val ;
#endif
{
	if ( w_io->rw_pos>BLOCK_SIZE-1 ) fatal("internal:2b ran out of block") ;
	w_io->content[w_io->rw_pos]= val & 0xFF ;
	w_io->rw_pos++ ;
}

/* Copy of the above, but with buffers */
void
#ifdef __STDC__
wio_1b(struct ba_list *p_io,int val)
#else
wio_1b(p_io,val)
        struct ba_list *p_io ;
        int	        val ;
#endif
{
	if ( p_io->rw_pos>BLOCK_SIZE-1 ) fatal("internal:2b ran out of block") ;
	p_io->content[p_io->rw_pos]= val & 0xFF ;
	p_io->rw_pos++ ;
}


int
#ifdef __STDC__
wbl_tell(void)
#else
wbl_tell()
#endif
{ return w_io->rw_pos ; }

void
#ifdef __STDC__
wbl_seek(int off)
#else
wbl_seek(off)
	int	off ;
#endif
{ w_io->rw_pos= off ; }

/* ----------------------------------------------------------------------
   All of the previous was devoted to writing blocks.
   The next part will be devoted to reading blocks.
 */
char *
#ifdef __STDC__
rio_string(struct ba_list *p_io)
#else
rio_string(p_io)
	struct ba_list *p_io ;
#endif
{
	register char	*src ;
	register byte	*dst ;

	src=(char *)(dst=p_io->content+p_io->rw_pos) ;
	while ( *dst && dst-p_io->content<BLOCK_SIZE-1 ) {
		dst++;
	}
	if ( *dst ) fatal("internal:string ran out of block") ;
	dst++;
	p_io->rw_pos= dst-p_io->content ;
	return src ;
}

byte *
#ifdef __STDC__
rio_mem(struct ba_list *p_io,int n)
#else
rio_mem(p_io,n)
	struct ba_list *p_io ;
	int		n;
#endif
{
	register byte	*src ;

	src=p_io->content+p_io->rw_pos ;
	p_io->rw_pos+= n ;
	if ( p_io->rw_pos>BLOCK_SIZE ) {
		fatal("internal:rbl_mem ran out of block") ;
	}
	return src ;
}

long
#ifdef __STDC__
rio_4b(struct ba_list *p_io)
#else
rio_4b(p_io)
	struct ba_list	*p_io ;
#endif
{
	register byte	*src ;
	register int	i = 4 ;
	register long	val ;

	if ( p_io->rw_pos>BLOCK_SIZE-4 ) fatal("internal:4b ran out of block") ;
	src=p_io->content+p_io->rw_pos ; val=0 ;
	while ( i-- ) {
		val <<= 8 ;
		val += byteval(*src++) ;
	}
	p_io->rw_pos += 4 ;
	return val ;
}

unsigned
#ifdef __STDC__
rio_2b(struct ba_list *p_io)
#else
rio_2b(p_io)
        struct ba_list  *p_io ;
#endif
{
	register byte	*src ;
	register int	i = 2 ;
	register unsigned	val ;

	if ( p_io->rw_pos>BLOCK_SIZE-2 ) fatal("internal:2b ran out of block") ;
	src=p_io->content+p_io->rw_pos ; val=0 ;
	while ( i-- ) {
		val <<= 8 ;
		val += byteval(*src++) ;
	}
	p_io->rw_pos += 2 ;
	return val ;
}

int
#ifdef __STDC__
rio_1b(struct ba_list *p_io)
#else
rio_1b(p_io)
        struct ba_list  *p_io ;
#endif
{
	register int	val ;

	if ( p_io->rw_pos>BLOCK_SIZE-1 ) fatal("internal:2b ran out of block") ;
	val=byteval(p_io->content[p_io->rw_pos]);
	p_io->rw_pos++ ;
	return val ;
}

void
#ifdef __STDC__
init_read(void)
#else
init_read()
#endif
{
	init_rio();
#ifdef MULTI_THREADED
	/* Initialize buffer admin */
	initpool(&readpool) ;
	if ( !free_init ) {
		initpool(&freepool) ;
		free_init=1 ;
	}
	if ( niobuf<1 ) {
		if ( niobuf<0 ) warning("# of buffers (-n) should be >=1") ;
		niobuf=1 ;
	}
	fillpool(&freepool, BLOCK_SIZE, niobuf );
	if ( niobuf>1 ) {
		if ( debug==1 || debug>2 ) {
			message("Starting read thread");
		}
		if ( thread_newthread(read_thread,STK_SIZE,(char *)0,0)==0 ) {
			warning("can not create read thread, running single threaded");
			r_single_threaded=1 ;
		}
	} else {
		r_single_threaded=1 ;
		r_io= get_block(&freepool) ;
	}
#else
	r_io= (struct ba_list *)getmem(sizeof (struct ba_list));
	r_io->content= getmem(BLOCK_SIZE) ;
#endif
}

int
read_block() {

	if ( eof_seen ) return -1 ;
#ifdef MULTI_THREADED
	if ( !r_single_threaded ) {
		if ( r_io ) (void)put_block(&freepool,r_io,AT_END) ;
		r_io= get_block(&readpool) ;
		if ( r_io->flags&BA_ABORT ) t_finish() ;
		if ( r_io->flags&BA_END ) {
			(void)put_block(&freepool,r_io,AT_END);
			eof_seen=1 ;
			r_io= (struct ba_list *)0;
			return -1 ;
		}
	} else
#endif /* MULTI_THREADED */
	{
		in_block(r_io) ;
		if ( r_io->flags&BA_ABORT ) t_finish() ;
		if ( r_io->flags&BA_END ) {
			eof_seen=1 ;
			return -1 ;
		}
	}
	if ( r_io->flags&BA_ERROR ) {
		return -1 ;
	}
	return rio_1b(r_io) ;
}

#ifdef MULTI_THREADED
static void
read_thread(p,s) char *p ; int s ; {
	struct ba_list	*my_buffer ;
	int		endit_flag ;

	endit_flag=0 ;
	do {
		my_buffer=get_block(&freepool) ;
		in_block(my_buffer) ;
		if ( my_buffer->flags&(BA_END|BA_ABORT) ) endit_flag =1 ;
		(void)put_block(&readpool,my_buffer,AT_END);
	} while (!endit_flag) ;
}

#endif /* MULTI_THREADED */

void iostats() {

	
#ifdef MULTI_THREADED
	message("io statistics: %d pool buffers",niobuf);
	if ( readpool.s_no!=0 ) {
		message("read queue length: mean %ld, max %d",
			readpool.s_total/readpool.s_no,
			readpool.s_max );
	}
	if ( writepool.s_no!=0 ) {
		message("write queue length: mean %ld, max %d",
			writepool.s_total/writepool.s_no,
			writepool.s_max );
	}
#endif
	rw_stats();
}
