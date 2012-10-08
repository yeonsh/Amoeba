/*	@(#)cmos.c	1.13	96/02/27 13:50:54 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/*
 * cmos.c
 *
 * The RT/CMOS RAM chip contains a real-time clock and 64 bytes of
 * CMOS RAM, the real-time clock uses 14 bytes, the rest of the RAM
 * is allocated to configuration information. This driver extracts
 * some interesting values from this non volatile memory.
 *
 * Author:
 *	Leendert van Doorn
 */
#include <map.h>
#include <amoeba.h>
#include <sys/proto.h>
#include <string.h>
#include <machdep.h>
#include <global.h>
#include <cmdreg.h>
#include <stderr.h>
#include <stdcom.h>
#include <file.h>
#include <time.h>
#include <timerdev.h>
#include <randseed.h>
#include <module/rnd.h>
#include <module/prv.h>
#include <module/mutex.h>
#include "i386_proto.h"

#include "cmos.h"

#ifndef NR_CMOSSVR_THREADS
#define NR_CMOSSVR_THREADS	2
#endif
#ifndef CMOSSVR_STKSIZ
#define CMOSSVR_STKSIZ	2200
#endif

static capability cmos_cap;	/* server's capability */

#ifdef PROFILE
static void cmos_prof_intr();
#endif

int cmos_read();
void cmos_write();

/*
 * Read hardware clock
 */
read_hardware_clock(secp, msecp)
    long *secp;
    int *msecp;
{
    struct tm tm;
    long unixtime();

    *msecp = 0;
    tm.tm_sec = cmos_read(CMOS_SECOND);
    tm.tm_min = cmos_read(CMOS_MINUTE);
    tm.tm_hour = cmos_read(CMOS_HOUR);
    tm.tm_mday = cmos_read(CMOS_DAY);
    tm.tm_mon = cmos_read(CMOS_MONTH);
    tm.tm_year = cmos_read(CMOS_YEAR);
    if ((cmos_read(CMOS_STATB) & CMOS_DM) == 0) {
	/* data stored in BCD format */
	tm.tm_sec = bcd2dec(tm.tm_sec);
	tm.tm_min = bcd2dec(tm.tm_min);
	tm.tm_hour = bcd2dec(tm.tm_hour);
	tm.tm_mday = bcd2dec(tm.tm_mday);
	tm.tm_mon = bcd2dec(tm.tm_mon);
	tm.tm_year = bcd2dec(tm.tm_year);
    }
    tm.tm_mon--;
    *secp = unixtime(&tm);
}

/*
 * Write hardware clock
 */
/* ARGSUSED */
write_hardware_clock(sec, msec)
    long sec;
    int msec;
{
    struct tm *tm;
    struct tm *gmtime();

    tm = gmtime(&sec);
    tm->tm_mon++;
    if ((cmos_read(CMOS_STATB) & CMOS_DM) == 0) {
	/* data stored in BCD format */
	tm->tm_sec = dec2bcd(tm->tm_sec);
	tm->tm_min = dec2bcd(tm->tm_min);
	tm->tm_hour = dec2bcd(tm->tm_hour);
	tm->tm_mday = dec2bcd(tm->tm_mday);
	tm->tm_mon = dec2bcd(tm->tm_mon);
	tm->tm_year = dec2bcd(tm->tm_year);
    }
    cmos_write(CMOS_SECOND, tm->tm_sec);
    cmos_write(CMOS_MINUTE, tm->tm_min);
    cmos_write(CMOS_HOUR, tm->tm_hour);
    cmos_write(CMOS_DAY, tm->tm_mday);
    cmos_write(CMOS_MONTH, tm->tm_mon);
    cmos_write(CMOS_YEAR, tm->tm_year);
}

/*
 * Initialize the RT/CMOS mutex
 */
void
cmos_init()
{
    /*
     * Re-initialize the realtime clock if necessary
     */
    if ((cmos_read(CMOS_STATD) & CMOS_VRB) == 0) {
	/* BIOS default settings */
	cmos_write(CMOS_STATA, CMOS_DV2 | CMOS_RS6);
	cmos_write(CMOS_STATB, CMOS_24HOUR);
	(void) cmos_read(CMOS_STATC);
	(void) cmos_read(CMOS_STATD);
    }

    /*
     * Some random bit we can spare. Should work for BCD
     * as well as binary format.
     */
    randseed((unsigned long) cmos_read(CMOS_SECOND), 8, RANDSEED_INVOC);
    randseed((unsigned long) cmos_read(CMOS_MINUTE), 8, RANDSEED_INVOC);
    randseed((unsigned long) cmos_read(CMOS_HOUR), 8, RANDSEED_INVOC);
#ifdef PROFILE
    setirq(CMOS_IRQ, cmos_prof_intr);
    /* enable interrupts when needed */
#endif
}

/*
 * Publish the capability and start CMOS server threads
 */
void
cmos_initthreads()
{
    register int i;
    capability pub_cap;
    void cmos_thread();

    /*
     * Publish the put capability of the server
     */
    uniqport(&cmos_cap.cap_port);
    uniqport(&cmos_cap.cap_priv.prv_random);
    cmos_cap.cap_priv.prv_rights = PRV_ALL_RIGHTS;
    pub_cap = cmos_cap;
    priv2pub(&cmos_cap.cap_port, &pub_cap.cap_port);
    dirappend("cmos", &pub_cap);
    for (i = 0; i < NR_CMOSSVR_THREADS; i++)
	NewKernelThread(cmos_thread, (vir_bytes) CMOSSVR_STKSIZ);
}

/*
 * This server allows you to inspect/modify the values in CMOS RAM
 */
void
cmos_thread()
{
    char *repptr, buffer[CMOS_SIZE];
    bufsize reqlen, replen;
    header hdr;
    int i;
    rights_bits rts;
    
    for (;;) {
	hdr.h_port = cmos_cap.cap_port;
#ifdef USE_AM6_RPC
	reqlen = rpc_getreq(&hdr, buffer, CMOS_SIZE);
#else
	reqlen = getreq(&hdr, buffer, CMOS_SIZE);
#endif
	if (ERR_STATUS(reqlen)) {
	    printf("cmos: getreq returned %d\n", reqlen);
	    continue;
	}

	replen = 0;
	repptr = NILBUF;
	if (prv_decode(&hdr.h_priv, &rts, &cmos_cap.cap_priv.prv_random) != 0)
	    hdr.h_status = STD_CAPBAD;
	else {
	    switch (hdr.h_command) {
	    case FSQ_READ:
		if (rts & RGT_READ) {
		    repptr = buffer;
		    hdr.h_extra = FSE_NOMOREDATA;
		    for (i = 0; i < hdr.h_size; i++) {
			if ((int)hdr.h_offset + i >= CMOS_SIZE) {
			    hdr.h_status = STD_ARGBAD;
			    break;
			}
			buffer[i] = cmos_read((int)hdr.h_offset + i);
		    }
		    if (i == hdr.h_size) {
			 replen = hdr.h_size;
			 hdr.h_status = STD_OK;
		    }
		} else
		    hdr.h_status = STD_DENIED;
		break;
	    case FSQ_WRITE:
		if (rts & RGT_WRITE) {
		    for (i = 0; i < hdr.h_size; i++) {
			if ((int)hdr.h_offset + i >= CMOS_SIZE) {
			    hdr.h_status = STD_ARGBAD;
			    break;
			}
			cmos_write((int)hdr.h_offset + i, buffer[i]);
		    }
		    if (i == hdr.h_size)
			 hdr.h_status = STD_OK;
		} else
		    hdr.h_status = STD_DENIED;
		break;
	    case STD_INFO:
		repptr = "CMOS server";
		hdr.h_size = replen = strlen(repptr);
		hdr.h_status = STD_OK;
		break;
	    default:
		hdr.h_status = STD_COMBAD;
		break;
	    }
	}
#ifdef USE_AM6_RPC
	rpc_putrep(&hdr, repptr, replen);
#else
	putrep(&hdr, repptr, replen);
#endif
    }
    /*NOTREACHED*/
}

#ifdef PROFILE
/*
 * Start profiling. We do out best effort to set the
 * rate to ``ival'' which is in msec granularity.
 */
void
cmos_prof_start(ival)
    long ival;
{
    register int statusa;
    int rate;

    /*
     * Best effort approximation. Too bad the
     * granularity isn't in micro-seconds
     */
    if (ival < 2)
	rate = 6; /* 1024 Hz, 0.976562 ms */
    else if (ival <= 3)
	rate = 7; /* 512 Hz, 1.953125 ms */
    else if (ival <= 7)
	rate = 8; /* 256 Hz, 3.90625 ms */
    else if (ival <= 13)
	rate = 9; /* 128 Hz, 7.8125 ms */
    else if (ival <= 24)
	rate = 10; /* 64 Hz, 15.625 ms */
    else if (ival <= 47)
	rate = 11; /* 32 Hz, 31.25 ms */
    else if (ival <= 94)
	rate = 12; /* 16 Hz, 62.5 ms */
    else if (ival <= 188)
	rate = 13; /* 8 Hz, 125 ms */
    else if (ival <= 375)
	rate = 14; /* 4 Hz, 250 ms */
    else
	rate = 15; /* 2 Hz, 500 ms */

    statusa = cmos_read(CMOS_STATA);
    cmos_write(CMOS_STATA, (statusa & ~0x0F) | rate);
    cmos_write(CMOS_STATB, cmos_read(CMOS_STATB) | CMOS_PIE);
    (void) cmos_read(CMOS_STATC);
    pic_enable(CMOS_IRQ);
}

/*
 * Stop profiling, reset timer to default interval
 */
void
cmos_prof_stop()
{
    register int statusa;

    pic_disable(CMOS_IRQ);
    statusa = cmos_read(CMOS_STATA);
    cmos_write(CMOS_STATA, (statusa & ~0x0F) | CMOS_RS6);
    cmos_write(CMOS_STATB, cmos_read(CMOS_STATB) & ~CMOS_PIE);
    (void) cmos_read(CMOS_STATC);
}

/*
 * Timer interrupt, save stack frame for profile usage.
 */
/* ARGSUSED */
static void
cmos_prof_intr(reason, frame)
    int reason;
    struct fault *frame;
{
    /* clear interrupt latch */
    out_byte(CMOS_ADDR, CMOS_STATC);
    (void) in_byte(CMOS_DATA);
    profile_timer(frame);
}
#endif /* PROFILE */

#define MAX_INB_LOOP	10000

/*
 * Read data from the CMOS chip
 */
int
cmos_read(addr)
    int addr;
{
    int flags, value;
    register int count;

    /*
     * When reading the RTC we have to wait when
     * there's an update in progress
     */
    flags = get_flags(); disable();
    if (addr < CMOS_STATA) {
	out_byte(CMOS_ADDR, CMOS_STATA);
	for (count = 0; count < MAX_INB_LOOP; count++) {
	    if ((in_byte(CMOS_DATA) & CMOS_UIP) == 0) {
		break;
	    }
	}
	if (count >= MAX_INB_LOOP) {
	    /* sanity check; shouldn't happen */
	    printf("cmos_read: update pending\n");
	}
    }
    out_byte(CMOS_ADDR, addr);
    value = in_byte(CMOS_DATA);
    set_flags(flags);
    return value;
}

/*
 * Write data to the CMOS chip.
 * The lowest 14 bytes of CMOS memory can only
 * be written if the update cylce of the real
 * time clock is turned off.
 */
void
cmos_write(addr, value)
    int addr, value;
{
    int flags, statusb;

    /* timer updates are special */
    flags = get_flags(); disable();
    if (addr < CMOS_STATA) {
	/* turn timer update cycle off */
	out_byte(CMOS_ADDR, CMOS_STATB);
	statusb = in_byte(CMOS_DATA);
	out_byte(CMOS_ADDR, CMOS_STATB);
	out_byte(CMOS_DATA, statusb | CMOS_SET);
    
	out_byte(CMOS_ADDR, addr);
	out_byte(CMOS_DATA, value);

	/* turn timer update cycle on */
	out_byte(CMOS_ADDR, CMOS_STATB);
	out_byte(CMOS_DATA, statusb);
    } else {
	out_byte(CMOS_ADDR, addr);
	out_byte(CMOS_DATA, value);
    }
    set_flags(flags);
}

/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

static int	mon_lengths[2][MONS_PER_YEAR] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int	year_lengths[2] = {
	DAYS_PER_NYEAR, DAYS_PER_LYEAR
};

#ifdef lint
#define	const
#endif

struct tm *
gmtime(clock)
const time_t *clock;
{
	register struct tm *	tmp;
	register long		days;
	register long		rem;
	register int		y;
	register int		yleap;
	register int *		ip;
	static struct tm	tm;

	tmp = &tm;
	days = *clock / SECS_PER_DAY;
	rem = *clock % SECS_PER_DAY;
	tmp->tm_hour = (int) (rem / SECS_PER_HOUR);
	rem = rem % SECS_PER_HOUR;
	tmp->tm_min = (int) (rem / SECS_PER_MIN);
	tmp->tm_sec = (int) (rem % SECS_PER_MIN);
	tmp->tm_wday = (int) ((EPOCH_WDAY + days) % DAYS_PER_WEEK);
	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYS_PER_WEEK;
	y = EPOCH_YEAR;
	if (days >= 0) {
		for ( ; ; ) {
			yleap = isleap(y);
			if (days < (long) year_lengths[yleap])
				break;
			++y;
			days = days - (long) year_lengths[yleap];
		}
	} else do {
		--y;
		yleap = isleap(y);
		days = days + (long) year_lengths[yleap];
	} while (days < 0);
	tmp->tm_year = y - TM_YEAR_BASE;
	tmp->tm_yday = (int) days;
	ip = mon_lengths[yleap];
	for (tmp->tm_mon = 0; days >= (long) ip[tmp->tm_mon]; ++(tmp->tm_mon))
		days = days - (long) ip[tmp->tm_mon];
	tmp->tm_mday = (int) (days + 1);
	tmp->tm_isdst = 0;
	return tmp;
}

/*
 * Reverse routine, written by <sater@cs.vu.nl>
 * and undoubtedly already under various names in the public domain
 */

long unixtime(tm)
struct tm *tm;
{
	int year,y;
	int yleap;
	long days;
	register int *ip;
	int m;
	long utime;

	year = tm->tm_year + TM_YEAR_BASE;
	days = 0;
	for (y = EPOCH_YEAR; y < year; y++) {
		yleap = isleap(y);
		days += year_lengths[yleap];
	}
	yleap = isleap(year);
	ip = mon_lengths[yleap];
	for (m = 0; m < tm->tm_mon; m++) {
		days += ip[m];
	}
	days += tm->tm_mday-1;
	utime = days * SECS_PER_DAY;
	utime += tm->tm_hour * SECS_PER_HOUR +
		 tm->tm_min * SECS_PER_MIN +
		 tm->tm_sec /* * SECS_PER_SEC :-) */ ;
	return utime;
}
