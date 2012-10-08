/*	@(#)Tginit.c	1.4	96/02/27 10:55:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * File		: Tginit.c
 *
 * Original	: March 9, 1992
 * Author(s)	: Irina 
 * Description	: Auxiliary procedures for test program  for file server.
 *



/* __TGINIT_C__ should be defined before including "Tginit.h". */
#define	__TGINIT_C_
#include	"Tginit.h"
#include	"module/rnd.h"


#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/* Some global flags controlling the output and tests. */
PUBLIC	bool	verbose;         	  /* Give lots of info on tests. */
PUBLIC  int     identg  = 0;              /* for threadalloc */
PUBLIC  int     identl  = 0;              /* for threadalloc */

#define def_catcher(signo, catcher_name) \
									\
PRIVATE void catcher_name(sig, us, extra) 				\
signum sig;								\
thread_ustate *us;							\
_VOIDSTAR extra;							\
{									\
    ABUFPRINT abuf = (ABUFPRINT) extra;					\
    longjmp(*(jmp_buf *)thread_alloc(abuf->aident, sizeof(jmp_buf)), 	\
	    (abuf->flag = signo));					\
}

/*ARGSUSED*/
def_catcher(EXC_ILL, catcher_EXC_ILL)
/*ARGSUSED*/
def_catcher(EXC_ODD, catcher_EXC_ODD)
/*ARGSUSED*/
def_catcher(EXC_MEM, catcher_EXC_MEM)
/*ARGSUSED*/
def_catcher(EXC_BPT, catcher_EXC_BPT)
/*ARGSUSED*/
def_catcher(EXC_INS, catcher_EXC_INS)
/*ARGSUSED*/
def_catcher(EXC_DIV, catcher_EXC_DIV)
/*ARGSUSED*/
def_catcher(EXC_FPE, catcher_EXC_FPE)
/*ARGSUSED*/
def_catcher(EXC_ACC, catcher_EXC_ACC)
/*ARGSUSED*/
def_catcher(EXC_SYS, catcher_EXC_SYS)
/*ARGSUSED*/
def_catcher(EXC_ARG, catcher_EXC_ARG)
/*ARGSUSED*/
def_catcher(EXC_EMU, catcher_EXC_EMU)
/*ARGSUSED*/
def_catcher(EXC_ABT, catcher_EXC_ABT)

PUBLIC void init_exception_flag(abuf)
ABUFPRINT abuf;
{
    abuf -> flag = 0;
    sig_catch(EXC_ILL, catcher_EXC_ILL, (_VOIDSTAR) abuf);
    sig_catch(EXC_ODD, catcher_EXC_ODD, (_VOIDSTAR) abuf);
    sig_catch(EXC_MEM, catcher_EXC_MEM, (_VOIDSTAR) abuf);
    sig_catch(EXC_BPT, catcher_EXC_BPT, (_VOIDSTAR) abuf);
    sig_catch(EXC_INS, catcher_EXC_INS, (_VOIDSTAR) abuf);
    sig_catch(EXC_DIV, catcher_EXC_DIV, (_VOIDSTAR) abuf);
    sig_catch(EXC_FPE, catcher_EXC_FPE, (_VOIDSTAR) abuf);
    sig_catch(EXC_ACC, catcher_EXC_ACC, (_VOIDSTAR) abuf);
    sig_catch(EXC_SYS, catcher_EXC_SYS, (_VOIDSTAR) abuf);
    sig_catch(EXC_ARG, catcher_EXC_ARG, (_VOIDSTAR) abuf);
    sig_catch(EXC_EMU, catcher_EXC_EMU, (_VOIDSTAR) abuf);
    sig_catch(EXC_ABT, catcher_EXC_ABT, (_VOIDSTAR) abuf);
}

#ifdef __STDC__
PUBLIC void
bufprint(BUFPRINT *abuf, char *format, ...)
#else
/*VARARGS2*/
PUBLIC void
bufprint(abuf, format, va_alist)
BUFPRINT *abuf;
char *format;
va_dcl
#endif
{
    char    buf[1024];
    char   *begin, *end;
    va_list args;

    begin = abuf->p;
    end = abuf->endi;

#ifdef __STDC__
    va_start(args, format);
#else
    va_start(args);
#endif
    (void) vsprintf(buf, format, args);
    va_end(args);
    if (begin == NULL || end == NULL) {
	printf("%s", buf);
    } else {
	char *p = buf;

	while (*p != '\0' && begin < end) {
	    *begin++ = *p++;
	}
	if (begin >= end) {
	    printf("buffer overflow\n");
	    fwrite(abuf->buf, (size_t) (abuf->endi - abuf->buf),
		   (size_t) 1, stdout);
	    /* reset pointer to start of buffer */
	    begin = abuf->buf;
	}

	*begin = 0;
	abuf->p = begin;
    }
}

PUBLIC void test_for_exception(abuf)
BUFPRINT * abuf;
{
    if (abuf ->flag != 0) {
	bufprint(abuf, "\n!!! exception : ");

	switch (abuf->flag) { 
	case EXC_ILL:
	    bufprint(abuf, "EXC_ILL - Illegal instruction\n");
	    break;
	case EXC_ODD:
	    bufprint(abuf, "EXC_ODD - Mis-aligned reference\n");
	    break;
	case EXC_MEM:
	    bufprint(abuf, "EXC_MEM - Non-existent memory\n");
	    break;
	case EXC_BPT:
	    bufprint(abuf, "EXC_BPT - Breakpoint instruction\n");
	    break;
	case EXC_INS:
	    bufprint(abuf, "EXC_INS - Undefined instruction\n");
	    break;
	case EXC_DIV:
	    bufprint(abuf, "EXC_DIV - Division by zero\n");
	    break;
	case EXC_FPE:
	    bufprint(abuf, "EXC_FPE - Floating exception\n");
	    break;
	case EXC_ACC:
	    bufprint(abuf, "EXC_ACC - Memory acces control violation\n");
	    break;
	case EXC_SYS:
	    bufprint(abuf, "EXC_SYS - Bad system call\n");
	    break;
	case EXC_ARG:
	    bufprint(abuf, "EXC_ARG - Illegal operand \n");
	    break;
	case EXC_EMU:
	    bufprint(abuf, "EXC_EMU - System call emulation\n");
	    break;
	case EXC_ABT:
	    bufprint(abuf, "EXC_ABT - abort() called\n");
	    break;
	default:
	    bufprint(abuf, "undocumented exception => %d\n");
	    break;
	}
    }
    abuf->flag = 0;
}


/* Checks if two capabilities refer to the same object. */

PUBLIC	bool	obj_cmp(cap1, cap2, abuf)
BUFPRINT * abuf;
capability	*cap1, *cap2;
{
    int	i;
    
    if (verbose) 
	bufprint(abuf, "----  obj_cmp() ----\n");
    
    for (i = 0; i < PORTSIZE; i++)
	if (cap1->cap_port._portbytes[i] != cap2->cap_port._portbytes[i])
	    return FALSE;
    
    if (prv_number(&cap1->cap_priv) == prv_number(&cap2->cap_priv))
	return TRUE;
    else
	return FALSE;
    
}  /* obj_cmp() */

PUBLIC void make_cap_check_corrupt(new_cap, old_cap)
capability *new_cap, *old_cap;
{
    *new_cap = *old_cap;
    new_cap -> cap_priv.prv_random._portbytes[0] ^= 1;
    
}

PUBLIC void make_cap_rights_corrupt(new_cap, old_cap)
capability *new_cap, *old_cap;
{
    *new_cap = *old_cap;
    new_cap -> cap_priv.prv_rights ^= 1;
}

PUBLIC  bool init_buffer(buf, abuf, s)
char * buf;
BUFPRINT * abuf;
b_fsize s;
{
    b_fsize  j, n;
    errstat  err;
    
    if (verbose)  
	bufprint(abuf,"----  init_buffer()  ----\n");
    
    /* Initialize first part of the buffer using the random number server. */
    n = MIN(MAX_RANDOM, s);
    err = rnd_getrandom(buf, (int) n);
    if (err != STD_OK){
	bufprint(abuf, "rnd_getrandom failed, err = %s\n", err_why(err));
	return FALSE;
    }

    /* To avoid overloading the random number with many simultaneous requests,
     * initialize the rest of the buffer using rand().
     */
    if (s > sizeof(unsigned int)) {
	unsigned int salt;

	(void) memmove((_VOIDSTAR) &salt, (_VOIDSTAR) buf, sizeof(salt));
	srand(salt);
    }

    for (j = n; j < s; j++) {
	buf[j] = rand() & 0xff;
    }

    return TRUE;
    
}/* init_buff() */


PUBLIC bool create_file(cap, buff, size, commit, newcap, new_dif_old, abuf)
capability * cap;
char * buff;
b_fsize size;
int commit;
capability * newcap;
bool new_dif_old;
ABUFPRINT abuf;
{
    errstat err;
    
    if (verbose) {    
	bufprint(abuf, "----  create_file(%s, safety %s)  ----\n",
		 ((commit&BS_COMMIT) == BS_COMMIT) ? "commited" : "uncommited",
		 ((commit&BS_SAFETY) == BS_SAFETY) ? "on" : "off");
	
    }
    if ((err = b_create(cap, buff, size, commit, newcap)) != STD_OK)
	if (!(((err == STD_NOMEM) | (err == STD_NOSPACE)) && ! new_dif_old)) {
	    bufprint(abuf, "\n create file failed, error = %s, size = %ld\n",
		     err_why(err), size);
	    bufprint(abuf,"\n file was %s, safety was %s\n",
		     ((commit&BS_COMMIT)==BS_COMMIT)?"commited" : "uncommited",
		     ((commit & BS_SAFETY) == BS_SAFETY) ? "on" : "off");
	    
	    return FALSE;
	}
    if (new_dif_old == cap_cmp(newcap, cap)){
	bufprint(abuf, "\nnew file created must have a %s capability\n",
		 new_dif_old ? "new" : "old");
	return FALSE;
    }
    
    return TRUE;
} /* create_file */

PUBLIC bool verify_file(bufw, bufr, size, offset, cap, equal, file_size, abuf)
BUFPRINT * abuf;
char * bufw, *bufr;
b_fsize size, offset;
capability * cap;
bool equal;
b_fsize file_size;
{
    b_fsize size_read;
    errstat err;
    
    size_read = NEG_ONE;
    if (verbose)
	bufprint(abuf,"----  verify_file()  ----\n");
    
    (void) memset((_VOIDSTAR) bufr, 0, (size_t) BIG_SIZE);
    
    if ((err = b_read(cap, offset, bufr, size, &size_read)) != STD_OK){
	bufprint(abuf, "read failed, err = %s, size = %d\n",
		 err_why(err), size);
	return FALSE;
    }
    if ((offset + size_read) > file_size){
	bufprint(abuf, "too much data, offset = %ld",offset),
	bufprint(abuf, "size read = %ld, file size = %ld\n",
		 size_read, file_size);
	return FALSE;
    }
    if (equal && (size != size_read) || (size_read > size)) {
	bufprint(abuf, "incorrect size, expected size = %ld, readsize = %ld\n",
		 size, size_read);
	return FALSE;
    }
    if (memcmp(bufr, bufw + offset, (size_t)size_read)){
	bufprint(abuf, "data read do not match original data, expected size="),
	bufprint(abuf, "%ld, size_read = %ld\n)", size, size_read);
	bufprint(abuf, "read= %d %d %d", bufr[0], bufr[1], bufr[2]);
	bufprint(abuf, " buff write = %d %d %d...\n",
		 bufw[0], bufw[1], bufw[2]);
	return FALSE;
    }
    return TRUE;
}  /* verify_file() */


PUBLIC bool cap_server(psvr_cap, name_server, abuf)
BUFPRINT * abuf;
capability * psvr_cap;
char * name_server;

/* Initialize the global variables. */

{
    errstat	err;
    
    if (verbose) 
	bufprint(abuf, "----  cap_server()  ----\n");
    
    /* Get the capability of the Bullet server. */
    if ((err = name_lookup(name_server, psvr_cap)) != STD_OK){
	bufprint(abuf, "name_look failed for %s, err = %s", 
		 name_server, err_why(err));
	return FALSE;
    }
    return TRUE;
    
} /* cap_server() */


PUBLIC bool execute_restrict(old_cap, new_cap, mask, name, cap_good, abuf)
capability * old_cap;
capability * new_cap;
rights_bits mask;
char * name;
bool cap_good;
ABUFPRINT abuf;
{
    errstat err;
    int size;
    char bufr[10];
    
    if (verbose)  
	bufprint(abuf, "----  execute_restrict()  ----\n");
    err = std_info(old_cap, bufr, 10, &size);
    if ((!cap_good && (err != STD_CAPBAD)) ||
	(cap_good && !(err==STD_OK || err==STD_DENIED || err==STD_OVERFLOW)))
    {
	bufprint(abuf, "capability is %s bad\n", cap_good ? "" : "not");
	bufprint(abuf, "err = %s\n", err_why(err));
	return FALSE;
    }
    
    err = std_restrict(old_cap, mask, new_cap);
    
    if (verbose && cap_good) {
	bufprint(abuf, "oldrights = 0x%x, mask = 0x%x, newrights = 0x%x\n",
		 old_cap->cap_priv.prv_rights, mask,
		 new_cap->cap_priv.prv_rights);
    }
    
    if (!cap_good && (err != STD_CAPBAD)) {
	bufprint(abuf, "std_restrict failed for "),
	bufprint(abuf, "bad capability, %s, err = %s\n", name, err_why(err));
	return FALSE;
    }
    if (cap_good && !obj_cmp(old_cap,new_cap, abuf)){
	bufprint(abuf, "std_restrict failed; the objects are different\n");
	return FALSE;
    }
    if (cap_good && (err != STD_OK)){
	bufprint(abuf, "std restrict() failed for %s, err = %s\n", 
		 name, err_why(err));
        return FALSE;
    }
    if (cap_good &&
	((old_cap->cap_priv.prv_rights & mask)!=new_cap->cap_priv.prv_rights))
    {
	bufprint(abuf, "std_restrict() failed, ");
	bufprint(abuf, "old rights = 0x%x, mask = 0x%x, new rights = 0x%x\n",
		 old_cap->cap_priv.prv_rights, mask, 
		 new_cap->cap_priv.prv_rights);
	return FALSE;
    }
    return TRUE;
} /* execute_restrict() */

/*  Tginit */



