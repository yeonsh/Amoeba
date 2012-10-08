/*	@(#)packet.h	1.1	92/06/25 14:49:24 */
/*
 * Packet layout definitions
 */

/* implementation constants */
#define	PKT_POOLSIZE	5
#define	PKT_DATASIZE	1514

/*
 * Structure of a packet.
 * Each packet can hold exactly one ethernet message.
 */
typedef struct {
    int pkt_used;			/* whether this packet it used */
    int pkt_len;			/* length of data  */
    uint8 *pkt_offset;			/* current offset in data */
    uint8 pkt_data[PKT_DATASIZE];	/* packet data */
} packet_t;

extern packet_t *pkt_alloc();
extern void pkt_release();
