/*	@(#)oneC_sum.c	1.1	96/02/27 14:14:53 */
/*
oneC_sum.c

Part of the internet checksum (see RFC 1071 Computing the Internet checksum)

Created:	June 1995 by Philip Homburg <philip@cs.vu.nl>
*/

#include <amoeba.h>
#include <sys/types.h>
#include <server/ip/tcpip.h>
#include <server/ip/types.h>
#include <server/ip/gen/oneCsum.h>

u16_t oneC_sum (prev, data_p, size)
u16_t prev;
u16_t *data_p;
size_t size;
{
	register unsigned long tmp;
	u8_t *bp;
	register u16_t *wp;
	u8_t word[2];
	int odd;
	void *data= data_p;

	if (size == 0)
		return prev;

	bp= data;
	tmp= prev;

	odd= (unsigned)bp & 1;
	if (odd)
	{
		tmp= ((tmp & 0xff) << 8) | ((tmp >> 8) & 0xff);
		word[0]= 0;
		word[1]= *bp++;
		tmp += *(u16_t *)word;
		size--;
	}
	wp= (u16_t *)bp;

	while(size >= 16)
	{
		tmp +=	wp[0] + wp[1] + wp[2] + wp[3] +
			wp[4] + wp[5] + wp[6] + wp[7];
		wp += 8;
		size -= 16;
	}

	while(size >= 2)
	{
		tmp += *wp++;
		size -= 2;
	}

	if (size)
	{
		word[0]= *(u8_t *)wp;
		word[1]= 0;
		tmp += *(u16_t *)word;
	}

	tmp= (tmp & 0xffff) + (tmp >> 16);
	if (tmp > 0xffff)
		tmp -= 0xffff;

	if (odd)
		tmp= ((tmp & 0xff) << 8) | ((tmp >> 8) & 0xff);
	
	return tmp;
}

/*
 * $PchHeader: /mount/hd2/minix/lib/ip/RCS/oneC_sum.c,v 1.3 1994/12/22 19:58:45 philip Exp philip $
 */
