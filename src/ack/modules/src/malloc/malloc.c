/*	@(#)malloc.c	1.1	91/04/11 12:59:13 */

/**********************************************************/
/*
/*		 This was file READ_ME
/*
/**********************************************************/

/*
	PROGRAM
		malloc(), free(), realloc()
	AUTHOR
		Dick Grune, Free University, Amsterdam
		Modified by Ceriel Jacobs, Free University, Amsterdam,
		to make it faster
	VERSION
		1.1 90/06/25 13:13:08
	DESCRIPTION
	This is an independent rewrite of the malloc/free package; it is
	fast and efficient.  Free blocks are kept in doubly linked lists,
	list N holding blocks with sizes between 2**N and 2**(N+1)-1.
	Consequently neither malloc nor free have to do any searching:
	the cost of a call of malloc() (or free()) is constant, however
	many blocks you have got.
	
	If you switch on the NON_STANDARD macro (see param.h) every block
	costs 2 pointers overhead (otherwise it's 4).
*/
/*
	There is an organisational problem here: during devellopment
	I want the package divided into modules, which implies external
	names for the communication.  The only external names I want in
	the finished product are malloc, realloc and free.  This requires
	some hanky-panky.
*/


/**********************************************************/
/*
/*		 This was file size_type.h
/*
/**********************************************************/

typedef unsigned int size_type;


/**********************************************************/
/*
/*		 This was file param.h
/*
/**********************************************************/

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

#	undef	NON_STANDARD	/*	If defined, the contents of a block
					will NOT be left undisturbed after it
					is freed, as opposed to what it says
					in the manual (malloc(2)).
					Setting this option reduces the memory
					overhead considerably.  I personally
					consider the specified behaviour an
					artefact of the original
					implementation.
				*/

#	undef	ASSERT		/*	If defined, some inexpensive tests
					will be made to ensure the
					correctness of some sensitive data.
					It often turns an uncontrolled crash
					into a controlled one.
				*/

#	undef	CHECK		/*	If defined, extensive and expensive
					tests will be done, inculding a
					checksum on the mallinks (chunk
					information blocks).  The resulting
					information will be printed on a file
					called mal.out .
					Additionally a function
						maldump(n) int n;
					will be defined, which will dump
					pertinent info in pseudo-readable
					form; it aborts afterwards if n != 0.
				*/

#	undef	EXTERN		/*	If defined, all static names will
					become extern, which is a help in
					using adb(1) or prof(1)
				*/

#	define	STORE		/*	If defined, separate free lists will
					be kept of chunks with small sizes,
					to speed things up a little.
				*/

#	define SYSTEM		/*	If defined, the system module is used.
					Otherwise, "sbrk" is called directly.
				*/

#define	ALIGNMENT	8
				/* alignment common to all types */
#define	LOG_MIN_SIZE	3
#define	LOG_MAX_SIZE	24


/**********************************************************/
/*
/*		 This was file impl.h
/*
/**********************************************************/

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/*	This file essentially describes how the mallink info block
	is implemented.
*/

#define	MIN_SIZE	(1<<LOG_MIN_SIZE)
#define	MAX_FLIST	(LOG_MAX_SIZE - LOG_MIN_SIZE)
#if ALIGNMENT != 4 && ALIGNMENT != 8 && ALIGNMENT != 16
ALIGNMENT must be 4, 8, or 16
#endif
#if MIN_SIZE % ALIGNMENT
ALIGNMENT must be a dividor of MIN_SIZE
#endif
#if ALIGNMENT == 4
#define LOG_ALIGNMENT 2
#endif
#if ALIGNMENT == 8
#define LOG_ALIGNMENT 3
#endif
#if ALIGNMENT == 16
#define LOG_ALIGNMENT 4
#endif
#define align(n)	(((n) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

union _inf {
	union _inf *ptr;
	unsigned int ui;
};

typedef union _inf _9_mallink;
#define	MAL_NULL	((_9_mallink *)0)

/*	Access macros; only these macros know where to find values.
	They are also lvalues.
*/
#ifndef	NON_STANDARD
#define	OFF_SET	0
#else	def NON_STANDARD
#define	OFF_SET	2
#endif	NON_STANDARD

#define	_log_prev_of(ml)	((ml)[-1+OFF_SET]).ptr
#define	_log_next_of(ml)	((ml)[-2+OFF_SET]).ptr
#define	_phys_prev_of(ml)	((ml)[-3+OFF_SET]).ptr
#define	_this_size_of(ml)	((ml)[-4+OFF_SET]).ui
#ifndef	CHECK
#define	N_WORDS			4
#else	ifdef	CHECK
#define	_checksum_of(ml)	((ml)[-5+OFF_SET]).ui
#define	_print_of(ml)		((ml)[-6+OFF_SET]).ui
#define	_mark_of(ml)		((ml)[-7+OFF_SET]).ui
#define	N_WORDS			7
#endif	CHECK

#define	_11_mallink_size()		(unsigned int) \
	align((N_WORDS - OFF_SET) * sizeof (_9_mallink))

#ifdef	CHECK
#define	set_mark(ml,e)		(_mark_of(ml) = (e))
#define	mark_of(ml)		(_mark_of(ml))

#define	set_checksum(ml,e)	(_checksum_of(ml) = (e))
#define	_4_checksum_of(ml)		(_checksum_of(ml))
#endif	CHECK

#define new_mallink(ml)		( _log_prev_of(ml) = 0, \
				  _log_next_of(ml) = 0, \
				  _phys_prev_of(ml) = 0, \
				  _this_size_of(ml) = 0 )

#define	block_of_mallink(ml)	((char *)ml)
#define	_10_mallink_of_block(addr)	((_9_mallink *)addr)

#define	public	extern
#define	publicdata	extern
#ifndef	EXTERN
#define	_12_private	static
#define	_13_privatedata	static
#else	def	EXTERN
#define	_12_private	extern
#define	_13_privatedata
#endif	EXTERN

#ifdef	ASSERT
_12_private m_assert();
#define	assert(b)		(!(b) ? m_assert(__FILE__, __LINE__) : 0)
#else	ndef	ASSERT
#define	assert(b)		0
#endif	ASSERT


/**********************************************************/
/*
/*		 This was file check.h
/*
/**********************************************************/

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#ifdef	CHECK

_12_private _1_check_mallinks(), calc_checksum(), check_work_empty();
_12_private started_working_on(), stopped_working_on();

#else	ifndef	CHECK

#define	maldump(n)		abort()
#define	_1_check_mallinks(s)	0
#define	calc_checksum(ml)	0
#define	started_working_on(ml)	0
#define	stopped_working_on(ml)	0
#define	check_work_empty(s)	0

#endif	CHECK


/**********************************************************/
/*
/*		 This was file log.h
/*
/**********************************************************/

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/*	Algorithms to manipulate the doubly-linked lists of free
	chunks.
*/

_12_private link_free_chunk(), unlink_free_chunk();
_12_private _9_mallink *first_present(), *search_free_list();

#ifdef STORE
#define in_store(ml)		((size_type)_phys_prev_of(ml) & STORE_BIT)
#define set_store(ml, e) \
	(_phys_prev_of(ml) = (_9_mallink *) \
		((e) ? (size_type) _phys_prev_of(ml) | STORE_BIT : \
		       (size_type) _phys_prev_of(ml) & ~STORE_BIT))
#endif
#define	_15_set_log_prev(ml,e)	(_log_prev_of(ml) = (e))
#define	log_prev_of(ml)		(_9_mallink *) (_log_prev_of(ml))

#define	_14_set_log_next(ml,e)	(_log_next_of(ml) = (e))
#define	log_next_of(ml)		(_9_mallink *) (_log_next_of(ml))



/**********************************************************/
/*
/*		 This was file phys.h
/*
/**********************************************************/

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
/*	Algorithms to manipulate the doubly-linked list of physical
	chunks.
*/
_13_privatedata _9_mallink *ml_last;

#define FREE_BIT		01
#ifdef STORE
#define STORE_BIT		02
#define BITS			(FREE_BIT|STORE_BIT)
#else
#define BITS			(FREE_BIT)
#endif

#define __bits(ml)		((size_type)_phys_prev_of(ml) & BITS)
#define	__free_of(ml)		((size_type)_phys_prev_of(ml) & FREE_BIT)
#define __phys_prev_of(ml)	(_9_mallink *)((size_type)_phys_prev_of(ml) & ~BITS)
#define prev_size_of(ml)	((char *)(ml) - \
				 (char *)__phys_prev_of(ml) - \
				 _11_mallink_size() \
				)
#define	_17_set_phys_prev(ml,e) \
	(_phys_prev_of(ml) = (_9_mallink *) ((char *)e + __bits(ml)))

#ifdef	CHECK
_12_private Error();
#define	phys_prev_of(ml)	(_9_mallink *) \
	(first_mallink(ml) ? \
		(char *)Error("phys_prev_of first_mallink %lx", "somewhere", (long)ml) : \
		(char *)__phys_prev_of(ml) \
	)
#else	ndef	CHECK
#define	phys_prev_of(ml)	__phys_prev_of(ml)
#endif	CHECK

#define	first_mallink(ml)	(int) (__phys_prev_of(ml) == 0)
#define	last_mallink(ml)	(int) ((ml) == ml_last)

/*	There is an ambiguity in the semantics of phys_next_of: sometimes
	one wants it to return MAL_NULL if there is no next chunk, at
	other times one wants the address of the virtual chunk at the
	end of memory.  The present version returns the address of the
	(virtual) chunk and relies on the user to test last_mallink(ml)
	first.
*/
#define size_of(ml)		(_this_size_of(ml) - _11_mallink_size())
#define	_16_set_phys_next(ml,e) \
	(_this_size_of(ml) = (unsigned int)((char *)(e) - (char *)(ml)))
#define	phys_next_of(ml)	(_9_mallink *) ((char *)(ml) + _this_size_of(ml))

#define	set_free(ml,e) \
	(_phys_prev_of(ml) = (_9_mallink *) \
		((e) ? (size_type) _phys_prev_of(ml) | FREE_BIT : \
		       (size_type) _phys_prev_of(ml) & ~FREE_BIT))
#define	free_of(ml)		(__free_of(ml))

#define _6_coalesce_forw(ml,nxt)	( unlink_free_chunk(nxt), \
				  combine_chunks((ml), (nxt)))

#define _5_coalesce_backw(ml,prv)	( unlink_free_chunk(prv), \
				  stopped_working_on(ml), \
				  combine_chunks((prv), (ml)), \
				  started_working_on(prv))

#ifdef	CHECK
#define	set_print(ml,e)		(_print_of(ml) = (e))
#define	print_of(ml)		(_print_of(ml))
#endif	CHECK

_12_private truncate(), combine_chunks();
_12_private _9_mallink *create_chunk();


/**********************************************************/
/*
/*		 This was file mal.c
/*
/**********************************************************/

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

/*	Malloc space is traversed by N doubly-linked lists of chunks, each
	containing a couple of house-keeping data addressed as a
	'mallink' and a piece of useful space, called the block.
	The N lists are accessed through their starting pointers in
	free_list[].  Free_list[n] points to a list of chunks between
	2**(n+LOG_MIN_SIZE) and 2**(n+LOG_MIN_SIZE+1)-1, which means
	that the smallest chunk is 2**LOG_MIN_SIZE (== MIN_SIZE).
*/

#ifdef SYSTEM
#include	<system.h>
#define SBRK	sys_break
#else
#define SBRK	sbrk
#define	ILL_BREAK		(char *)(-1)	/* funny failure value */
#endif
extern char *SBRK();
#ifdef STORE
#define	MAX_STORE	32
#define MAX_SZ_IN_STORE	(MAX_STORE*ALIGNMENT)
_12_private do_free(), sell_out();
_13_privatedata _9_mallink *store[MAX_STORE];
#endif STORE

char *
malloc(n)
	register unsigned int n;
{_1_check_mallinks("malloc entry");{
	register _9_mallink *ml;
	register int min_class;

	if (n == 0) {
		return 0;
	}
	if (n < MIN_SIZE) n = align(MIN_SIZE); else n = align(n);
#ifdef STORE
	if (n <= MAX_SZ_IN_STORE)	{
		/* look in the store first */
		register _9_mallink **stp = &store[(n >> LOG_ALIGNMENT) - 1];
		
		if (ml = *stp)	{
			*stp = log_next_of(ml);
			set_store(ml, 0);
			_1_check_mallinks("malloc fast exit");
			assert(! in_store(ml));
			return block_of_mallink(ml);
		}
	}
#endif STORE

	check_work_empty("malloc, entry");

	/*	Acquire a chunk of at least size n if at all possible;
		Try everything.
	*/
	{
		/*	Inline substitution of "smallest".
		*/
		register unsigned int n1 = n;

		assert(n1 < (1L << LOG_MAX_SIZE));
		min_class = 0;

		do {
			n1 >>= 1;
			min_class++;
		} while (n1 >= MIN_SIZE);
	}

	if (min_class >= MAX_FLIST)
		return (char *) 0;	/* we don't deal in blocks that big */
	ml = first_present(min_class);
	if (ml == MAL_NULL)	{
		/*	Try and extend */
		register char *p;
#define	GRABSIZE	4096		/* Power of 2 */
		register unsigned int req =
			((MIN_SIZE<<min_class)+ _11_mallink_size() + GRABSIZE - 1) &
				~(GRABSIZE-1);
	
		if (!ml_last)	{
			/* first align SBRK() */
		
			p = SBRK(0);
			SBRK((int) (align((size_type) p) - (size_type) p));
		}

		p = SBRK((int)req);
		if (p == ILL_BREAK) {
			req = n + _11_mallink_size();
			p = SBRK((int)req);
		}
		if (p == ILL_BREAK)	{
			/*	Now this is bad.  The system will not give us
				more memory.  We can only liquidate our store
				and hope it helps.
			*/
#ifdef STORE
			sell_out();
			ml = first_present(min_class);
			if (ml == MAL_NULL)	{
#endif STORE
				/* In this emergency we try to locate a suitable
				   chunk in the free_list just below the safe
				   one; some of these chunks may fit the job.
				*/
				ml = search_free_list(min_class - 1, n);
				if (!ml)	/* really out of space */
					return (char *) 0;
				started_working_on(ml);
				unlink_free_chunk(ml);
				_1_check_mallinks("suitable_chunk, forced");
#ifdef STORE
			}
			else started_working_on(ml);
#endif STORE
		}
		else {
			assert((size_type)p == align((size_type)p));
			ml = create_chunk(p, req);
		}
		_1_check_mallinks("suitable_chunk, extended");
	}
	else started_working_on(ml);

	/* we have a chunk */
	set_free(ml, 0);
	calc_checksum(ml);
	_1_check_mallinks("suitable_chunk, removed");
	n += _11_mallink_size();
	if (n + MIN_SIZE <= size_of(ml)) {
		truncate(ml, n);
	}
	stopped_working_on(ml);
	_1_check_mallinks("malloc exit");
	check_work_empty("malloc exit");
#ifdef STORE
	assert(! in_store(ml));
#endif
	return block_of_mallink(ml);
}}

free(addr)
	char *addr;
{_1_check_mallinks("free entry");{
	register _9_mallink *ml;
	
	if (addr == 0) {
		_1_check_mallinks("free(0) very fast exit");
		return;
	}
	ml = _10_mallink_of_block(addr);

#ifdef STORE

	if (free_of(ml) || in_store(ml))
		return;				/* user frees free block */
	if (size_of(ml) <= MAX_SZ_IN_STORE)	{
		/* return to store */
		_9_mallink **stp = &store[(size_of(ml) >> LOG_ALIGNMENT) - 1];
		
		_14_set_log_next(ml, *stp);
		*stp = ml;
		set_store(ml, 1);
		calc_checksum(ml);
		_1_check_mallinks("free fast exit");
	}
	else	{
		do_free(ml);
		_1_check_mallinks("free exit");
	}
}}

_12_private
do_free(ml)
	register _9_mallink *ml;
{{
#endif

#ifndef STORE
	if (free_of(ml))	return;
#endif STORE
	started_working_on(ml);
	set_free(ml, 1);
	calc_checksum(ml);
	if (! last_mallink(ml)) {
		register _9_mallink *next = phys_next_of(ml);

		if (free_of(next)) _6_coalesce_forw(ml, next);
	}

	if (! first_mallink(ml)) {
		register _9_mallink *prev = phys_prev_of(ml);

		if (free_of(prev)) {
			_5_coalesce_backw(ml, prev);
			ml = prev;
		}
	}
	link_free_chunk(ml);
	stopped_working_on(ml);
	check_work_empty("free");

	/* Compile-time checks on param.h */
	switch (0)	{
	case MIN_SIZE < OFF_SET * sizeof(_9_mallink):	break;
	case 1:	break;
	/*	If this statement does not compile due to duplicate case
		entry, the minimum size block cannot hold the links for
		the free blocks.  Either raise LOG_MIN_SIZE or switch
		off NON_STANDARD.
	*/
	}
	switch(0)	{
	case sizeof(char *) != sizeof(size_type):	break;
	case 1:	break;
	/*	If this statement does not compile due to duplicate
		case entry, size_type is not defined correctly.
		Redefine and compile again.
	*/
	}
}}

char *
realloc(addr, n)
	char *addr;
	register unsigned int n;
{_1_check_mallinks("realloc entry");{
	register _9_mallink *ml, *ph_next;
	register unsigned int size;

	if (addr == 0) {
		/*	Behave like most Unix realloc's when handed a
			null-pointer
		*/
		return malloc(n);
	}
	if (n == 0) {
		free(addr);
		return 0;
	}
	ml = _10_mallink_of_block(addr);
	if (n < MIN_SIZE) n = align(MIN_SIZE); else n = align(n);
#ifdef STORE
	if (in_store(ml)) {
		register _9_mallink *stp = store[(size_of(ml) >> LOG_ALIGNMENT) - 1];
		_9_mallink *stp1 = 0;
		while (ml != stp)	{
			stp1 = stp;
			stp = log_next_of(stp);
		}
		stp = log_next_of(stp);
		if (! stp1) store[(size_of(ml) >> LOG_MIN_SIZE) - 1] = stp;
		else _14_set_log_next(stp1, stp);
		set_store(ml, 0);
		calc_checksum(ml);
	}
#endif
	if (free_of(ml)) {
		unlink_free_chunk(ml);
		set_free(ml, 0);		/* user reallocs free block */
	}
	started_working_on(ml);
	size = size_of(ml);
	if (	/* we can simplify the problem by adding the next chunk: */
		n > size &&
		!last_mallink(ml) &&
		(ph_next = phys_next_of(ml), free_of(ph_next)) &&
		n <= size + _11_mallink_size() + size_of(ph_next)
	)	{
		/* add in the physically next chunk */
		unlink_free_chunk(ph_next);
		combine_chunks(ml, ph_next);
		size = size_of(ml);
		_1_check_mallinks("realloc, combining");
	}
	if (n > size)	{		/* this didn't help */
		char *new;
		register char *l1, *l2 = addr;

		stopped_working_on(ml);
		if (!(new = l1 = malloc(n))) return (char *) 0;	/* no way */
		while (size--) *l1++ = *l2++;
		free(addr);
		check_work_empty("mv_realloc");
#ifdef STORE
		assert(! in_store(_10_mallink_of_block(new)));
#endif
		return new;
	}
	/* it helped, but maybe too well */
	n += _11_mallink_size();
	if (n + MIN_SIZE <= size_of(ml)) {
		truncate(ml, n);
	}
	stopped_working_on(ml);
	_1_check_mallinks("realloc exit");
	check_work_empty("realloc");
#ifdef STORE
	assert(! in_store(ml));
#endif
	return addr;
}}

/*	Auxiliary routines */

#ifdef STORE
_12_private
sell_out()	{
	/*	Frees all block in store.
	*/
	register _9_mallink **stp;
	
	for (stp = &store[0]; stp < &store[MAX_STORE]; stp++)	{
		register _9_mallink *ml = *stp;
		
		while (ml)	{
			*stp = log_next_of(ml);
			set_store(ml, 0);
			do_free(ml);
			ml = *stp;
		}
	}

}
#endif STORE

#ifdef	ASSERT
_12_private
m_assert(fn, ln)
	char *fn;
{
	char ch;
	
	while (*fn)
		write(2, fn++, 1);
	write(2, ": malloc assert failed in line ", 31);
	ch = (ln / 100) + '0'; write(2, &ch, 1); ln %= 100;
	ch = (ln / 10) + '0'; write(2, &ch, 1); ln %= 10;
	ch = (ln / 1) + '0'; write(2, &ch, 1);
	write(2, "\n", 1);
	maldump(1);
}
#endif	ASSERT


/**********************************************************/
/*
/*		 This was file log.c
/*
/**********************************************************/

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

/*	Logical manipulations.
	The chunks are properly chained in the physical chain.
*/

_13_privatedata _9_mallink *_7_free_list[MAX_FLIST];

_12_private
link_free_chunk(ml)
	register _9_mallink *ml;
{
	/*	The free chunk ml is inserted in its proper logical
		chain.
	*/
	register _9_mallink **mlp = &_7_free_list[-1];
	register unsigned int n = size_of(ml);
	register _9_mallink *ml1;

	assert(n < (1L << LOG_MAX_SIZE));

	do {
		n >>= 1;
		mlp++;
	}
	while (n >= MIN_SIZE);

	ml1 = *mlp;
	_15_set_log_prev(ml, MAL_NULL);
	_14_set_log_next(ml, ml1);
	calc_checksum(ml);
	if (ml1) {
		/* link backwards
		*/
		_15_set_log_prev(ml1, ml);
		calc_checksum(ml1);
	}
	*mlp = ml;
}

_12_private
unlink_free_chunk(ml)
	register _9_mallink *ml;
{
	/*	Unlinks a free chunk from (the middle of) the
		logical chain.
	*/
	register _9_mallink *next = log_next_of(ml);
	register _9_mallink *prev = log_prev_of(ml);

	if (!prev)	{
		/* it is the first in the chain */
		register _9_mallink **mlp = &_7_free_list[-1];
		register unsigned int n = size_of(ml);

		assert(n < (1L << LOG_MAX_SIZE));
		do {
			n >>= 1;
			mlp++;
		}
		while (n >= MIN_SIZE);
		*mlp = next;
	}
	else	{
		_14_set_log_next(prev, next);
		calc_checksum(prev);
	}
	if (next) {
		_15_set_log_prev(next, prev);
		calc_checksum(next);
	}
}

_12_private _9_mallink *
search_free_list(class, n)
	unsigned int n;
{
	/*	Searches the free_list[class] for a chunk of at least size n;
		since it is searching a slightly undersized list,
		such a block may not be there.
	*/
	register _9_mallink *ml;
	
	for (ml = _7_free_list[class]; ml; ml = log_next_of(ml))
		if (size_of(ml) >= n)
			return ml;
	return MAL_NULL;		/* nothing found */
}

_12_private _9_mallink *
first_present(class)
	int class;
{
	/*	Find the index i in free_list[] such that:
			i >= class && free_list[i] != MAL_NULL.
		Return MAL_NULL if no such i exists;
		Otherwise, return the first block of this list, after
		unlinking it.
	*/
	register _9_mallink **mlp, *ml;

	for (mlp = &_7_free_list[class]; mlp < &_7_free_list[MAX_FLIST]; mlp++) {
		if ((ml = *mlp) != MAL_NULL)	{
	
			*mlp = log_next_of(ml);	/* may be MAL_NULL */
			if (*mlp) {
				/* unhook backward link
				*/
				_15_set_log_prev(*mlp, MAL_NULL);
				calc_checksum(*mlp);
			}
			return ml;
		}
	}
	return MAL_NULL;
}

#ifdef	CHECK
_12_private _9_mallink *
_8_free_list_entry(i)	{
	/*	To allow maldump.c access to log.c's private data.
	*/
	return _7_free_list[i];
}
#endif	CHECK


/**********************************************************/
/*
/*		 This was file phys.c
/*
/**********************************************************/

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */

/*	Physical manipulations.
	The blocks concerned are not in any logical chain.
*/

_12_private _9_mallink *
create_chunk(p, n)
	char *p;
	unsigned int n;
{
	/*	The newly acquired piece of memory at p, of length n,
		is turned into a free chunk, properly chained in the
		physical chain.
		The address of the chunk is returned.
	*/
	register _9_mallink *ml;
	/*	All of malloc memory is followed by a virtual chunk, the
		mallink of which starts mallink_size() bytes past the last
		byte in memory.
		Its use is prevented by testing for ml == ml_last first.
	*/
	register _9_mallink *last = ml_last;
	
	assert(!last || p == (char *)phys_next_of(last) - _11_mallink_size());
	ml = (_9_mallink *)(p + _11_mallink_size());	/* bump ml */
	new_mallink(ml);
	started_working_on(ml);
	set_free(ml, 1);
	_17_set_phys_prev(ml, last);
	ml_last = ml;

	_16_set_phys_next(ml, (_9_mallink *)((char *)ml + n));
	calc_checksum(ml);
	assert(size_of(ml) + _11_mallink_size() == n);
	if (last && free_of(last)) {
		_5_coalesce_backw(ml, last);
		ml = last;
	}
	_1_check_mallinks("create_chunk, phys. linked");
	return ml;
}

_12_private
truncate(ml, size)
	register _9_mallink *ml;
	unsigned int size;
{
	/*	The chunk ml is truncated.
		The chunk at ml is split in two.
		The remaining part is then freed.
	*/
	register _9_mallink *new = (_9_mallink *)((char *)ml + size);
	register _9_mallink *ph_next = phys_next_of(ml);

	new_mallink(new);
	set_free(new, 1);
	_17_set_phys_prev(new, ml);
	_16_set_phys_next(new, ph_next);
	calc_checksum(new);
	if (! last_mallink(ml))	{
		_17_set_phys_prev(ph_next, new);
		calc_checksum(ph_next);
		if (free_of(ph_next)) _6_coalesce_forw(new, ph_next);
	}
	else	ml_last = new;
	_16_set_phys_next(ml, new);
	calc_checksum(ml);

	started_working_on(new);
	link_free_chunk(new);
	stopped_working_on(new);
	_1_check_mallinks("truncate");
}

_12_private
combine_chunks(ml1, ml2)
	register _9_mallink *ml1, *ml2;
{
	/*	The chunks ml1 and ml2 are combined.
	*/
	register _9_mallink *ml3 = phys_next_of(ml2);

	_16_set_phys_next(ml1, ml3);
	calc_checksum(ml1);
	if (!last_mallink(ml2))	{
		_17_set_phys_prev(ml3, ml1);
		calc_checksum(ml3);
	}
	if (ml_last == ml2)
		ml_last = ml1;
}


/**********************************************************/
/*
/*		 This was file check.c
/*
/**********************************************************/

/*
 * (c) copyright 1987 by the Vrije Universiteit, Amsterdam, The Netherlands.
 * See the copyright notice in the ACK home directory, in the file "Copyright".
 */
#include	<stdio.h>

#ifdef	CHECK			/* otherwise this whole file is skipped */

_12_private acquire_malout(), _2_check_ml_last();
_12_private dump_all_mallinks(), dump_free_list(), dump_mallink(), print_loop();
_12_private working_on();
_12_private unsigned int _3_checksum();
static FILE *malout;

_12_private _9_mallink *_8_free_list_entry();

#define	for_free_list(i,p) \
	for (p = _8_free_list_entry(i); p; p = log_next_of(p))

#define	for_all_mallinks(ml)	/* backwards! */ \
	for (ml = ml_last; ml; \
		ml = first_mallink(ml) ? MAL_NULL : phys_prev_of(ml))

/* Maldump */

static int pr_cnt = 0;

maldump(n)	{
	/*	Dump pertinent info in pseudo-readable format;
		abort afterwards if n != 0.
	*/
	static int dumping = 0;
	int i;
	
	if (dumping)
		return;
	dumping++;
	acquire_malout();
	fprintf(malout,
		">>>>>>>>>>>>>>>> DUMP OF ALL MALLINKS <<<<<<<<<<<<<<<<");
	fprintf(malout, "    ml_last = %ld\n", (long)ml_last);
	if (++pr_cnt == 100) pr_cnt = 0;
	dump_all_mallinks();
	fprintf(malout,
		">>>>>>>>>>>>>>>> DUMP OF FREE_LISTS <<<<<<<<<<<<<<<<\n");
	if (++pr_cnt == 100) pr_cnt = 0;
	for (i = 0; i < MAX_FLIST; i++)
		dump_free_list(i);
	fprintf(malout,
		">>>>>>>>>>>>>>>> END OF DUMP <<<<<<<<<<<<<<<<\n");
	fclose(malout);
	dumping--;
	if (n)
		abort();
}

_12_private
acquire_malout()	{
	static char buf[BUFSIZ];
	
	if (!malout)	{
		malout = freopen("mal.out", "w", stderr);	
		setbuf(malout, buf);
	}
}

_12_private
dump_all_mallinks()	{
	_9_mallink *ml;
	
	for_all_mallinks (ml)	{
		if (print_loop(ml))
			return;
		dump_mallink((char *)0, ml);
	}
}

_12_private
dump_free_list(i)	{
	_9_mallink *ml = _8_free_list_entry(i);
	
	if (!ml)
		return;
	fprintf(malout, "%2d: ", i);
	for_free_list(i, ml)	{
		if (print_loop(ml))
			return;
		fprintf(malout, "%ld ", ml);
	}
	fprintf(malout, "<\n");
}

_12_private int
print_loop(ml) _9_mallink *ml;	{
	if (print_of(ml) == pr_cnt)	{
		fprintf(malout, "... PRINT LOOP\n");
		return 1;
	}
	set_print(ml, pr_cnt);
	return 0;
}

_12_private
dump_mallink(s, ml) char *s; _9_mallink *ml;	{
	acquire_malout();
	if (s)
		fprintf(malout, "%s: ", s);
	fprintf(malout, "@: %ld;", (long)ml);
	if (ml && _4_checksum_of(ml) != _3_checksum(ml))
		fprintf(malout, ">>>> CORRUPTED <<<<");
	if (!ml)	{
		fprintf(malout, "\n");
		return;
	}	
	if (free_of(ml))	{
		fprintf(malout, " l_p: %ld;", (long)_log_prev_of(ml));
		fprintf(malout, " l_n: %ld;", (long)_log_next_of(ml));
	}
	fprintf(malout, " p_s: %ld;", (long)prev_size_of(ml));
	fprintf(malout, " t_s: %ld;", (long)_this_size_of(ml));
	fprintf(malout, " sz: %ld;", (long)size_of(ml));
	fprintf(malout, " fr: %d;", free_of(ml));
	fprintf(malout, "\n");
}

/*	Check_mallinks() checks the total data structure as accessible
	through free_list[] and ml_last.  All check_sums should be OK,
	except those held in the small array off_colour.  This is a
	trick to allow to continue checking even when a few mallinks
	are temporarily out of order.
	Check_mallinks() tests for a lot of internal consistency.
*/

/* Some arbitrary constants */
#define	IN_ML_LAST	93
#define	IN_FREE_LIST	57		/* and in ml_last */
#define	CLEAR		21

#define	VRIJ		1
#define	BEZET		2

_12_private
_1_check_mallinks(s) char *s;	{
	_9_mallink *ml;
	unsigned int size;
	int i;
	char stat;
	
	_2_check_ml_last(s);
	stat = BEZET;
	for_all_mallinks(ml)	{
		if (_4_checksum_of(ml) != _3_checksum(ml))
			Error("mallink info at %lx corrupted", s, (long)ml);
		if (working_on(ml))	{
			stat = BEZET;
			continue;
		}
		if (	!last_mallink(ml) &&
			phys_prev_of(phys_next_of(ml)) != ml
		)
			Error("upward chain bad at %lx", s, (long)ml);
		if (	!first_mallink(ml) &&
			phys_next_of(phys_prev_of(ml)) != ml
		)
			Error("downward chain bad at %lx", s, (long)ml);
		if (free_of(ml))	{
			if (stat == VRIJ)
				Error("free mallink at %lx follows free mallink",
								s, (long)ml);
			stat = VRIJ;
		}
		else
			stat = BEZET;
		set_mark(ml, IN_ML_LAST);
	}
	
	for (i = 0, size = MIN_SIZE; i < MAX_FLIST; i++, size *= 2)	{
		for_free_list(i, ml)	{
			if (working_on(ml))
				continue;
			if (!free_of(ml))
				Error("occupied mallink %lx occurs in free_list", s, (long)ml);
			switch (mark_of(ml))	{
			case IN_ML_LAST:
				set_mark(ml, IN_FREE_LIST);
				break;
			case IN_FREE_LIST:
				Error("mallink %lx occurs in 2 free_lists",
								s, (long)ml);
			default:
				Error("unknown mallink %lx in free_list",
								s, (long)ml);
			}
			if (size_of(ml) < size)
				Error("size of mallink %lx too small", s, (long)ml);
			if (size_of(ml) >= 2*size)
				Error("size of mallink %lx too large", s, (long)ml);
		}
	}
	for_all_mallinks (ml)	{
		if (working_on(ml))
			continue;
		if (free_of(ml) && mark_of(ml) != IN_FREE_LIST)
			Error("free mallink %lx is in no free_list", s, (long)ml);
		set_mark(ml, CLEAR);
	}
}

_12_private
_2_check_ml_last(s) char *s;	{
	if (ml_last && _this_size_of(ml_last) == 0)
		Error("size of ml_last == 0, at %ld", s, ml_last);
}

_12_private unsigned int
_3_checksum(ml) _9_mallink *ml;	{
	unsigned int sum = 0;
	
	if (free_of(ml))	{
		sum += (unsigned int)_log_prev_of(ml);
		sum += (unsigned int)_log_next_of(ml);
	}
	sum += (unsigned int)prev_size_of(ml);
	sum += (unsigned int)_this_size_of(ml);
	return sum;
}

_12_private
calc_checksum(ml) _9_mallink *ml;	{
	set_checksum(ml, _3_checksum(ml));
}

#define	N_COLOUR	10
static _9_mallink *off_colour[N_COLOUR];

_12_private
started_working_on(ml) _9_mallink *ml;	{
	int i;
	
	for (i = 0; i < N_COLOUR; i++)
		if (off_colour[i] == MAL_NULL)	{
			off_colour[i] = ml;
			return;
		}
	Error("out of off_colour array at %ld", "started_working_on", ml);
}

_12_private
stopped_working_on(ml) _9_mallink *ml;	{
	int i;
	
	for (i = 0; i < N_COLOUR; i++)
		if (off_colour[i] == ml)	{
			off_colour[i] = MAL_NULL;
			return;
		}
	Error("stopped working on mallink %ld", "stopped_working_on", ml);
}

_12_private int
working_on(ml) _9_mallink *ml;	{
	int i;
	
	for (i = 0; i < N_COLOUR; i++)
		if (off_colour[i] == ml)
			return 1;
	return 0;
}

_12_private
check_work_empty(s) char *s;	{
	int i;
	int cnt = 0;
	
	for (i = 0; i < N_COLOUR; i++)
		if (off_colour[i] != MAL_NULL)
			cnt++;
	if (cnt != 0)
		Error("off_colour not empty", s, MAL_NULL);
}

_12_private int
Error(fmt, s, ml) char *fmt, *s; _9_mallink *ml;	{
	setbuf(stdout, (char *) 0);
	printf("%s: ", s);
	printf(fmt, (long)ml);
	printf("\n");
	acquire_malout();
	fprintf(malout, "%s: ", s);
	fprintf(malout, fmt, (long)ml);
	fprintf(malout, "\n");
	fflush(stdout);
	maldump(1);
	return 0;			/* to satisfy lint */
}

#endif	CHECK

