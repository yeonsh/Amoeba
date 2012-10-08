/*	@(#)f30sm.c	1.3	94/04/06 09:04:52 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#include "amoeba.h"
#include "machdep.h"
#include "global.h"
#include "map.h"
#include "assert.h"
INIT_ASSERT

#include "gsminfo.h"
#include "smif.h"

/* Addresses where the device can be allocated. Make sure that there is
 * enough space per device!
 */
#define         DVADR_SM1               0x003F0000  
#define         DVADR_SM2               0x003F0100  
#define         DVADR_SM3               0x003F0200
#define         DVADR_SM4               0x003F0300

/* Address for mailbox */
#define 	MAILBOXADDR		0xFFD80000
#define		MAILBOXSPACE		0xFBFF0000

/* Mail box vectors */
#define         MAILBOXVEC0             192
#define         MAILBOXVEC1             193


/* Miscellaneous constants. */
#define		SIZEADDRSPACE		0x04000000
#define		MAXLEGALADDR		0x3FFFFF
#define         NNODE                   2 
#define 	NPKT			100
#define         PACKETSIZE              1600
#define         BASENODE                2 	/* armada00 is node 2 */


phys_bytes f30_sm_addr_conv(node, a)
int node;
phys_bytes a;
{
    phys_bytes r;

    if(a > 0 && a < MAXLEGALADDR)
	r = 0x7C000000 + node * SIZEADDRSPACE + a;
    else
	r = a;

    return(r);
}


void disablemail(vec)
    int vec;
{
    int n = vec - MAILBOXVEC0;
    long a = MAILBOXADDR + (n << 2);
    
    * (long *) a = 0x0;
}


void sendinterrupt(src, dst)
    int src;
    int dst;
{
    long space, addr, mailbox;
    int i = 0;
    
    assert(dst != src);
    space = MAILBOXSPACE;
    addr = (0x80 + dst) << 8;
    mailbox = space | addr;
    while(i < 1 && (* (long *) mailbox) & 0xFF != 0) i++;
    if(i == 1) printf("sendinterrupt: could not generate interrupt\n");
}


int smaddr(arg, ident)
    int arg;
    int *ident;
{
    *ident = fga_slotnumber();
    return(1);
}


extern char *malloc();

static int networkspec1[NNODE] = {0+BASENODE, 1+BASENODE};
static int networkspec2[NNODE] = {2+BASENODE, 3+BASENODE};
static int networkspec3[NNODE] = {3+BASENODE, 4+BASENODE};
static int networkspec4[NNODE] = {4+BASENODE, 5+BASENODE};
static int networkspec5[NNODE] = {13+BASENODE, 14+BASENODE};

int sm_alloc();
int sm_init();
void sm_send();
void sm_broadcast();


#define NEL(ar) (sizeof(ar)/sizeof(ar[0]))

sminfo_t sm_info[] = 
{
{ DVADR_SM1, MAILBOXVEC0, 14+BASENODE, NPKT, PACKETSIZE, smaddr, -1,  malloc,
     f30_sm_addr_conv, NNODE, networkspec5 },
}; 
    

hmi_t hmilist[] = 
{
{ "sm1", NEL(sm_info), sm_alloc, sm_init, sm_send, sm_broadcast },
{ 0 }
};
