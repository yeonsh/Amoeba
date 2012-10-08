/*	@(#)tod.c	1.3	96/02/27 13:57:25 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "global.h"
#include "machdep.h"
#include "map.h"
#include "randseed.h"
#include "sys/proto.h"
#include "openprom.h"
#include "debug.h"
#include "memaccess.h"
#include "mmuproto.h"
#include "assert.h"
INIT_ASSERT

struct hwtimeval {
    volatile unsigned char	htv_todcontrol;
    volatile unsigned char	htv_seconds;
    volatile unsigned char	htv_minutes;
    volatile unsigned char	htv_hours;
    volatile unsigned char	htv_weekday;
    volatile unsigned char	htv_date;
    volatile unsigned char	htv_month;
    volatile unsigned char	htv_year;
};

#define TODCTL_W	0x80
#define TODCTL_R	0x40
#define TODCTL_S	0x20

struct idprom {
    unsigned char	idp_format;	/* Always 1 */
    unsigned char	idp_machid;
    unsigned char	idp_eaddr[6];
    unsigned long	idp_manf_date;
    unsigned char	idp_serial[3];
    unsigned char	idp_chksum;
    unsigned char	idp_res[16];
};

static char *eeprom;
static struct idprom *idprom;
static struct hwtimeval *hwtvp;

#ifdef EXTREMELY_DANGEROUS
/* Sample code to change idprom on our Tatung boards.
   Only for use by brave men/women, who know what they are doing
   NB. On the Tatung machines the last 3 bytes of the ethernet address
       and the serial # are the same.  This may not be true on Suns, etc.
*/

patchup_idprom(idprom)
struct idprom *idprom;
{
    unsigned char *ucp;
    int i;
    unsigned char chksum;

    ucp = &idprom->idp_format;
    chksum = 0;
    for(i=0; i<sizeof(*idprom); i++) {
	chksum ^= ucp[i];
    }
    printf("idprom chksum check: xor is %x, chksum is %x\n", chksum, idprom->idp_chksum);
    if (idprom->idp_eaddr[3] == 0xFB &&
        idprom->idp_eaddr[4] == 0xF1 &&
        idprom->idp_eaddr[5] == 0xD5) {
	printf("This is the beast\n");
	idprom->idp_eaddr[3] = idprom->idp_serial[0] = 0xF5;
	idprom->idp_eaddr[4] = idprom->idp_serial[1] = 0x01;
	idprom->idp_eaddr[5] = idprom->idp_serial[2] = 0xD5;
	idprom->idp_chksum = 0;
	for(i=0; i<sizeof(*idprom); i++) {
	    chksum ^= ucp[i];
	}
	idprom->idp_chksum = chksum;
    }
}
#endif

#if ! defined(NO_KERNEL_SECURITY)
/*
 * Return cnt bytes of the eeprom in buf, starting at offset.
 * Status is 1 if ok and 0 if it overran the eeprom.
 */
int
eeprom_read(offset, buf, cnt)
int   offset;
uint8 *       buf;
int   cnt;
{
    char * ep;

    ep = eeprom + offset;
    if (ep >= (char *) idprom)
	return 0;
    (void) memcpy(buf, ep, (size_t) cnt);
    return 1;
}

#endif /* NO_KERNEL_SECURITY */


/* Routine called via obp_regnode to locate the MK48T device */

static void
mk48t_reg(np)
nodelist *	np;
{
    dev_reg_t	d;
#ifndef NDEBUG
    static int ndevs;

    ndevs++;
    assert (ndevs == 1);
#endif /* NDEBUG */
    obp_devaddr(np, &d);
    eeprom = (char *) mmu_mapdev(&d);
    idprom = (struct idprom *) (eeprom + d.reg_size - 40);
    hwtvp = (struct hwtimeval *) (eeprom + d.reg_size - 8);
#ifdef EXTREMELY_DANGEROUS
    patchup_idprom(idprom);
#endif
}


/*
 * initmk48t() (ENTRY POINT) -- initialize anything that the clock might need.
 */
void
initmk48t() {

    obp_regnode("name", "eeprom", mk48t_reg);

    /* Just some random bits we got anyway */
    randseed((unsigned long) hwtvp->htv_seconds, 7, RANDSEED_INVOC);
    randseed((unsigned long) hwtvp->htv_minutes, 7, RANDSEED_INVOC);
    randseed((unsigned long) hwtvp->htv_hours, 6, RANDSEED_INVOC);
}

/*
 * Get the Ethernet addr from idprom.
 */
void
idprom_eaddr(eaddr)
char * eaddr;
{
    int i;
    for (i = 0; i < 6; i++)
	*eaddr++ = idprom->idp_eaddr[i];
}

/*
 * rest of the routines here use the Mostek CMOS storage for a
 * calendar clock.
 */

#include "time.h"
#include "timerdev.h"

#define CHIP0YEAR	68

bcd2dec(b)
unsigned char b;
{
	return ((int)(b & 0xF0) >> 4) * 10 +(b & 0xF);
}

unsigned char
dec2bcd(d)
int d;
{
	return ((d / 10) << 4) + d % 10;
}

/* Forward decl */
long unixtime();

read_hardware_clock( secp, msecp )
long *secp;
int *msecp;
{
    struct tm tm;

    *msecp = 0;
    hwtvp->htv_todcontrol = TODCTL_R;
    tm.tm_sec = bcd2dec(hwtvp->htv_seconds);
    tm.tm_min = bcd2dec(hwtvp->htv_minutes);
    tm.tm_hour = bcd2dec(hwtvp->htv_hours);
    tm.tm_mday = bcd2dec(hwtvp->htv_date);
    tm.tm_mon = bcd2dec(hwtvp->htv_month) - 1;
    tm.tm_year = bcd2dec(hwtvp->htv_year) + CHIP0YEAR;
    hwtvp->htv_todcontrol = 0;
    DPRINTF(1, ("rtm = %d, %d, %d, %d, %d, %d\n", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec));
    *secp = unixtime( &tm );
}

/*ARGSUSED*/
write_hardware_clock( sec, msec )
long sec;
int msec;
{
    struct tm *tm;
    struct tm *gmtime();

    tm = gmtime( &sec );
    DPRINTF(1, ("wtm = %d, %d, %d, %d, %d, %d\n", tm->tm_year, tm->tm_mon, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec));
    hwtvp->htv_todcontrol = TODCTL_W;
    hwtvp->htv_seconds = dec2bcd(tm->tm_sec);
    hwtvp->htv_minutes = dec2bcd(tm->tm_min);
    hwtvp->htv_hours = dec2bcd(tm->tm_hour);
    hwtvp->htv_date = dec2bcd(tm->tm_mday);
    hwtvp->htv_month = dec2bcd(tm->tm_mon + 1);
    hwtvp->htv_year = dec2bcd(tm->tm_year - CHIP0YEAR);
    hwtvp->htv_todcontrol = 0;
}


/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

static int mon_lengths[ 2 ][ MONS_PER_YEAR ] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
};

static int year_lengths[ 2 ] = {
    DAYS_PER_NYEAR, DAYS_PER_LYEAR,
};

struct tm *gmtime( clock )
#ifdef __STDC__
const
#endif
time_t *clock;
{
    struct tm *tmp;
    long days, rem;
    int y, yleap, *ip;
    static struct tm tm;

    tmp = &tm;
    days = *clock / SECS_PER_DAY;
    rem = *clock % SECS_PER_DAY;
    tmp->tm_hour = (int) ( rem / SECS_PER_HOUR );
    rem = rem % SECS_PER_HOUR;
    tmp->tm_min = (int) ( rem / SECS_PER_MIN );
    tmp->tm_sec = (int) ( rem % SECS_PER_MIN );
    tmp->tm_wday = (int) (( EPOCH_WDAY + days ) % DAYS_PER_WEEK );
    if ( tmp->tm_wday < 0 )
	tmp->tm_wday += DAYS_PER_WEEK;

    y = EPOCH_YEAR;
    if ( days >= 0 ) {
	for ( ;; ) {
	    yleap = isleap( y );
	    if ( days < (long) year_lengths[ yleap ])
		break;
	    ++y;
	    days = days - (long) year_lengths[ yleap ];
	}
    } else do {
	--y;
	yleap = isleap( y );
	days = days + (long) year_lengths[ yleap ];
    } while ( days < 0 );

    tmp->tm_year = y - TM_YEAR_BASE;
    tmp->tm_yday = (int) days;
    ip = mon_lengths[ yleap ];
    for ( tmp->tm_mon = 0; days >= (long) ip[ tmp->tm_mon ]; ++(tmp->tm_mon) )
	days = days - (long) ip[ tmp->tm_mon ];
    tmp->tm_mday = (int) (days + 1);
    tmp->tm_isdst = 0;
    return( tmp );
}

/*
 * Reverse routine, written by <sater@cs.vu.nl>
 * and undoubtedly already under various names in the public domain
 */
long unixtime( tm )
struct tm *tm;
{
    int year, y;
    int yleap;
    long days;
    register int *ip;
    int m;
    long utime;

    year = tm->tm_year + TM_YEAR_BASE;
    days = 0;
    for ( y = EPOCH_YEAR; y < year; y++ ) {
	yleap = isleap( y );
	days += year_lengths[ yleap ];
    }

    yleap = isleap( year );
    ip = mon_lengths[ yleap ];
    for ( m = 0; m < tm->tm_mon; m++ ) {
	days += ip[ m ];
    }
    days += tm->tm_mday - 1;

    utime = days * SECS_PER_DAY;
    utime += tm->tm_hour * SECS_PER_HOUR +
		tm->tm_min * SECS_PER_MIN + tm->tm_sec;
    return( utime );
}
