/*	@(#)kgdb.c	1.3	94/04/06 08:55:31 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * kgdb.c
 *
 * This module allows you to use GDB to debug Amoeba kernels. Currently the
 * stubs are only invoked when a trap occurs, but in the near future this
 * should change so that running kernels can be debugged too.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <stddef.h>
#include <amoeba.h>
#include <machdep.h>
#include <assert.h>
INIT_ASSERT
#include <fault.h>
#include <exception.h>
#include <map.h>
#include <i80386.h>
#include <trap.h>
#include <type.h>
#include <bool.h>

#define BUFSIZ		1024		/* buffer size */
#define	REGISTER_BYTES	(16 * sizeof(int))

extern void breakpoint();

static void exception();
static int fromhex();
static int tohex();
static int hextomem();
static void memtohex();
static char *hextoint();

static char *getpkt();
static void putpkt();

int kgdb_enabled = FALSE;		/* kernel GDB enabled */

/*
 * Initialize the kernel GDB module. When kgdb_init is provided
 * this routine will immediatly jump into the debugger so that
 * initialization routines can be debugged too.
 */
void
kgdb_init()
{
    if (kernel_option("kgdb")) {
	kgdb_enabled = TRUE;
	kgdbio_init();
	/*
	 * In case the GDB client started before us,
	 * ACK any packets (presumably "$?#xx") sitting there.
	 */
	kgdbio_putc('+');
	if (kernel_option("kgdb_init"))
	    breakpoint();
    }
}

/*
 * This routine is called from the trap or interrupt handler
 * when a breakpoint instruction is encountered or a single
 * step operation completes.
 */
int 
kgdb_trap(reason, fp)
    int reason;
    struct fault *fp;
{
    static char obuf[BUFSIZ], ibuf[BUFSIZ];
    register char *iptr;
    uint32 addr, length;
    int regs[16];
    char cmd;

#ifdef CRT_TRACE
    asy_puts("\nKERNEL DEBUGGER ACTIVATED.\n");
    asy_puts("Waiting for GDB to connect ...\n");
#endif

    /* tell the host that we have stopped */
    exception(reason);

    /*
     * Kernel GDB command loop
     */
    while (TRUE) {
	iptr = getpkt(ibuf);

	switch (cmd = *iptr++) {

	/*
	 * Return last signal
	 */
	case '?':
	    exception(reason);
	    break;

	/*
	 * Read CPU registers
	 */
	case 'g':
	    /*
	     * In the event there's no privilege transitions we make
	     * up our own ss and esp. Note that if_faddr is NOT the
	     * stack pointer at the moment of the trap, but 32 bytes
	     * further.
	     */
	    if (fp->if_cs == K_CS_SELECTOR) {
		regs[4] = fp->if_faddr;
		regs[11] = fp->if_ds & 0xFFFF;
	    } else {
		regs[4] = fp->if_esp;
		regs[11] = fp->if_ss & 0xFFFF;
	    }
	    regs[0] = fp->if_eax;
	    regs[1] = fp->if_ecx;
	    regs[2] = fp->if_edx;
	    regs[3] = fp->if_ebx;
	    regs[5] = fp->if_ebp;
	    regs[6] = fp->if_esi;
	    regs[7] = fp->if_edi;
	    regs[8] = fp->if_eip;
	    regs[9] = fp->if_eflag;
	    regs[10] = fp->if_cs & 0xFFFF;
	    regs[12] = fp->if_ds & 0xFFFF;
	    regs[13] = fp->if_es & 0xFFFF;
	    regs[14] = fp->if_fs & 0xFFFF;
	    regs[15] = fp->if_gs & 0xFFFF;
	    memtohex((char *)regs, obuf, REGISTER_BYTES);
	    putpkt(obuf);
	    break;

	/*
	 * Write CPU registers
	 */
	case 'G':
	    if (hextomem(iptr, regs, REGISTER_BYTES)) {
		if (fp->if_cs == K_CS_SELECTOR) {
		    fp->if_faddr = regs[4];
		    /* ds == ss */
		} else {
		    fp->if_esp = regs[4];
		    fp->if_ss = regs[11] & 0xFFFF;
		}
		fp->if_eax = regs[0];
		fp->if_ecx = regs[1];
		fp->if_edx = regs[2];
		fp->if_ebx = regs[3];
		fp->if_ebp = regs[5];
		fp->if_esi = regs[6];
		fp->if_edi = regs[7];
		fp->if_eip = regs[8];
		fp->if_eflag = regs[9];
		fp->if_cs = regs[10] & 0xFFFF;
		fp->if_ds = regs[12] & 0xFFFF;
		fp->if_es = regs[13] & 0xFFFF;
		fp->if_fs = regs[14] & 0xFFFF;
		fp->if_gs = regs[15] & 0xFFFF;
		framedump(fp, regs[4], regs[11]);
		putpkt("OK");
	    } else
		putpkt("E01");
	    break;
                        
	/*
	 * Write memory
	 */
	case 'M':
	    if (iptr = hextoint(iptr, &addr)) {
		if (*iptr++ == ',') {
		    if (iptr = hextoint(iptr, &length)) {
			if (*iptr++ == ':') {
			    if (!probe(addr, length)) {
				printf("kgdb: probe address %x length %d failed\n",
				    addr, length);
				putpkt("E02");
				continue;
			    }
			    hextomem(iptr, addr, length);
			    putpkt("OK");
			    iptr = 0;
			}
		    }
		}
	    }
	    if (iptr) {
		putpkt("E01");
		break;
	    }
	    break;

	/*
	 * Read memory
	 */
	case 'm':
	    if (iptr = hextoint(iptr, &addr)) {
		if (*iptr++ == ',') {
		    if (iptr = hextoint(iptr, &length)) {
			if (!probe(addr, length)) {
			    printf("kgdb: probe address %x length %d failed\n",
				addr, length);
			    putpkt("E02");
			    continue;
			}
			memtohex(addr, obuf, length);
			putpkt(obuf);
			iptr = 0;
		    }
		}
	    }
	    if (iptr) {
		putpkt("E01");
		break;
	    }
	    break;

	/*
	 * Continue
	 */
	case 'c':
	    /* FALL THROUGH */

	/*
	 * Single step
	 */
	case 's':
	    /* try to read optional EIP argument */
	    if (iptr = hextoint(iptr, &addr)) {
		if (!probe(addr, sizeof(addr))) {
		    putpkt("E02");
		    break;
		}
		fp->if_eip = addr;
	    }

	    /* set the trace bit when we're single stepping */
	    if (cmd == 's')
		fp->if_eflag |= EFL_TF;
	    else
		fp->if_eflag &= ~EFL_TF;
	    return 1; /* just continue */

	/*
	 * Kill the kernel
	 */
	case 'k':
	     abort();
	}
    }
}

/*
 * Tell the host that we've stopped due to the specified signal.
 */
static void
exception(reason)
    int reason;
{
    register int sigval;
    char buf[4];

    switch (reason) {
    case TRAP_DIVIDE:		/* division by zero */
	sigval = 8; break;
    case TRAP_DEBUG:		/* debug exceptions */
	sigval = 5; break;
    case TRAP_BREAKPOINT:	/* breakpoint */
	sigval = 5; break;
    case TRAP_OVERFLOW:		/* overflow */
	sigval = 16; break;
    case TRAP_BOUNDS:		/* bounds check */
	sigval = 16; break;
    case TRAP_INVALIDOP:	/* invalid opcode */
	sigval = 4; break;
    case TRAP_NDPNA:		/* NDP not available */
	sigval = 8; break;
    case TRAP_DOUBLEFAULT:	/* double fault */
	sigval = 7; break;
    case TRAP_OVERRUN:		/* NDP segment overrun (386 only)*/
	sigval = 11; break;
    case TRAP_INVALIDTSS:	/* invalid TSS */
	sigval = 11; break;
    case TRAP_SEGNOTPRESENT:	/* segment not present */
	sigval = 11; break;
    case TRAP_STACKFAULT:	/* stack fault */
	sigval = 11; break;
    case TRAP_GENPROT:		/* general protection */
	sigval = 11; break;
    case TRAP_PAGEFAULT:	/* page fault */
	sigval = 11; break;
    case TRAP_NDPERROR:		/* NDP error */
	sigval = 7; break;
    case TRAP_ALIGNMENT:	/* alignment check (486 only) */
	sigval = 11; break;
    default:			/* "software generated" */
	sigval = 7; break;
    }
    buf[0] = 'S';
    buf[1] = tohex(sigval >> 4);
    buf[2] = tohex(sigval);
    buf[3] = '\0';
    putpkt(buf);
}

/*
 * Read a packet from the remote machine. The format of
 * the packet is "$data#checksum", where data is a sequence
 * of hex-decimal characters and checksum a two complement
 * checksum of the data.
 */
static char *
getpkt(buf)
    char *buf;
{
    uint8 csum, xsum;
    register char *p;
    register int i;
    int ch;

    for (ch = kgdbio_getc();;) {

	while (ch != '$')
	    ch = kgdbio_getc();

	csum = 0;
	for (i = 0, p = buf; i < BUFSIZ; i++) {
	    if ((ch = kgdbio_getc()) == '$' || ch == '#')
		break;
	    *p++ = ch;
	    csum += ch;
	}
	*p = '\0';

	if (ch != '#')
	    continue;

	xsum = fromhex(kgdbio_getc()) << 4;
        xsum += fromhex(kgdbio_getc());
        if (xsum == csum)
	    break;
	kgdbio_putc('-');
    }
    kgdbio_putc('+');
    return buf;
}

/*
 * Send a packet to the remote machine
 */
static void
putpkt(buf)
    char *buf;
{
    register char *p;
    uint8 csum = 0;

    kgdbio_putc('$');
    for (p = buf; *p; p++) {
	kgdbio_putc(*p);
	csum += *p;
    }
    kgdbio_putc('#');
    kgdbio_putc(tohex(csum >> 4));
    kgdbio_putc(tohex(csum));
}

/*
 * Convert a string of hex digits to memory
 */
static int
hextomem(ptr, addr, size)
    char *ptr, *addr;
    int size;
{
    register int i;

    for (i = 0; i < size; i++) {
	if (ptr[0] == '\0' || ptr[1] == '\0')
	     return FALSE;
	*addr++ = fromhex(ptr[0]) << 4 | fromhex(ptr[1]);
	ptr += 2;
    }
    return TRUE;
}

/*
 * Convert memory to a string of hex digits
 */
static void
memtohex(addr, ptr, size)
    char *addr, *ptr;
    int size;
{
    register int i;

    for (i = 0; i < size; i++) {
	*ptr++ = tohex(addr[i] >> 4);
	*ptr++ = tohex(addr[i]);
    }
    *ptr = '\0';
}

/*
 * Convert a hex number into an integer,
 * stop at first non hex-decimal character
 */
static char *
hextoint(ptr, value)
    char *ptr;
    int *value;
{
    register char *p = ptr;

    *value = 0;
    while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f')) {
	*value = *value << 4 | fromhex(*p);
	p++;
    }
    return ptr == p ? NULL : p;
}

/*
 * Convert a hex digit to an integer
 */
static int
fromhex(ch)
    int ch;
{
    if (ch >= 'a' && ch <= 'f')
	return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
	return ch - 'A' + 10;
    if (ch >= '0' && ch <= '9')
	return ch - '0';
    return -1;
}

/*
 * Convert an integer into a hex digit
 */
static int
tohex(n)
    int n;
{
    n &= 0x0F;
    return n < 10 ? '0' + n : 'a' + n - 10;
}
