/*	@(#)tftp.h	1.1	91/04/10 10:52:49 */
/*
 * tftp packet on top of UDP, see RFC 783
 */

struct tftp_pkt {
	uint16	tftp_opcode;
	union {
		struct tftpreq {
			char tftp_fnammod[514];
		} tftp_req;
		struct tftpdat {
			uint16 tftp_blockno;
			char tftp_data[512];
		} tftp_dat;
		struct tftperr {
			uint16 tftp_errorcode;
			char tftp_errmsg[512];
		} tftp_err;
	} tftp_operand;
};

#define TFTP_OPC_RRQ	1
#define TFTP_OPC_WRQ	2
#define TFTP_OPC_DATA	3
#define TFTP_OPC_ACK	4
#define TFTP_OPC_ERROR	5

#define UDP_TFTP	69
