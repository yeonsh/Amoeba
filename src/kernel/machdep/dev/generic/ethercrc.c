/*	@(#)ethercrc.c	1.3	94/04/06 09:08:39 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef NOMULTICAST
/*
 * Compute the 32-bit Ethernet/IEEE 802.3 cyclic redundancy check.
 *
 * For transmission, the returned value must be complemented and then
 * sent low-order byte first (i.e., right to left).
 *
 * On reception, if buf ends with a correct CRC, the returned value
 * will be 0xdebb20e3.
 *
 * The AMD Lance Ethernet chip uses the high-order (leftmost) six bits of
 * the CRC of a multicast address as a hash index for multicast filtering.
 *
 * The Intel 82586 Ethernet chip uses a different six bits as a multicast
 * address hash.  Numbering the bits from 0 on the left to 31 on the right,
 * the Intel hash index is bits 2 to 7, in the order 432765.  How perverse.
 *
 * Written by Steve Deering, Stanford University, March 1989.
 */
#include <assert.h>
INIT_ASSERT

void computemask(val, mask)
    int val;
    unsigned short *mask;
{
    int i = 0;
    
    assert(val < 64 && val >= 0);
    while(val >= 16) {
	val -= 16;
	mask[i++] = 0;
    }
    mask[i++] = (1 << val);
    for(; i < 4; i++) mask[i] = 0;
}


unsigned long EtherCRC( buf, len )
    register unsigned char *buf;
    register int len;
{
    
    register unsigned long crc;
    register unsigned char byte;
    register int b;
    
    crc = 0xffffffff;
    while( len-- > 0 )
    {
	byte = *buf++;
	for( b = 0; b < 8; b++ )
	{
	    if( (byte & 0x1) ^ (crc & 0x1) )
	    {
		crc >>= 1;
		crc ^= 0xedb88320;
	    }
	    else
	    {
		crc >>= 1;
	    }
	    byte >>= 1;
	}
    }
    return( crc );
}
#endif
