/*	@(#)const.h	1.4	96/02/27 14:14:23 */
/* const.h */

#ifndef TCPIP_CONST_H
#define TCPIP_CONST_H

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef NDEBUG
#undef NDEBUG
#endif

#define NW_SUSPEND	(-999)
#define NW_OK		(0)
#define NW_WOULDBLOCK	(-998)

#define PUBLIC
#define EXTERN	extern
#define FORWARD	static
#define PRIVATE static

#define FALSE	0
#define TRUE	1

/* disable IP pseudo devices */
#define CONF_NO_PSIP			1

/* enable network byte order for length fields in the udp_io_hdr */
#define CONF_UDP_IO_NW_BYTE_ORDER	1

/* Buffers */
#define BUF_USEMALLOC	1
#define BUF32K_NR		16
#define BUF2K_NR		128
#define BUF512_NR		128
#define BUF_S			(32*1024)

/* The time returned by sys_milli is converted to a new value that
 * takes care of timer wraparound.  We shift the least significant
 * ERA_BITS out, and put the current era in the most significant bits.
 */
#define	ERA_BITS	4
#define	ERA_MASK	0xf0000000
#define	ERA_NUM		(1 << ERA_BITS)
#define	HZ		(1000 / ERA_NUM)

#define CLOCK_GRAN	1	/* in HZ */
  
#define where()		printf("%s, %d: ", __FILE__, __LINE__)

#endif /* TCPIP_CONST_H */
