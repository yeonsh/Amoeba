/*	@(#)aha154X.h	1.2	94/04/06 09:16:20 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

#ifndef __AHA154X_H__
#define	__AHA154X_H__

/* control register bits */
#define AHA_HRST	0x80	/* bit 7 - Hard Reset */
#define AHA_SRST	0x40	/* bit 6 - Soft Reset */
#define AHA_IRST	0x20	/* bit 5 - Interrupt Reset */
#define AHA_SCRST	0x10	/* bit 4 - SCSI Bus Reset */
/*			0x08	 * bit 3 - Reserved (set to 0) */
/*			0x04	 * bit 2 - Reserved (set to 0) */
/*			0x02	 * bit 1 - Reserved (set to 0) */
/*			0x01	 * bit 0 - Reserved (set to 0) */

/* status register bits */
#define AHA_STST	0x80	/* bit 7 - Self Test in Progress */
#define AHA_DIAGF	0x40	/* bit 6 - Internal Diagnostic Failure */
#define AHA_INIT	0x20	/* bit 5 - Mailbox Initialization Required */
#define AHA_IDLE	0x10	/* bit 4 - SCSI Host Adapter Idle */
#define AHA_CDF		0x08	/* bit 3 - Command/Data Out Port Full */
#define AHA_DF		0x04	/* bit 2 - Data In Port Full */
/*			0x02	 * bit 1 - Reserved */
#define AHA_INVDCMD	0x01	/* bit 0 - Invalid Host Adapter Command */

/* interrupt flags register bits */
#define AHA_ANYINT	0x80	/* bit 7 - Any Interrupt */
/*			0x40	 * bit 6 - Reserved */
/*			0x20	 * bit 5 - Reserved */
/*			0x10	 * bit 4 - Reserved */
#define AHA_SCRD	0x08	/* bit 3 - SCSI Reset Detected */
#define AHA_HACC	0x04	/* bit 2 - Host Adapter Command Complete */
#define AHA_MBOE	0x02	/* bit 1 - Mailbox Out Empty */
#define AHA_MBIF	0x01	/* bit 0 - Mailbox In Full */

/* AHA Command Codes */
#define AHACOM_INITBOX		0x01	/* Mailbox Initialization */
#define AHACOM_STARTSCSI	0x02	/* Start SCSI Command */
#define AHACOM_HAINQUIRY	0x04	/* Host Adapter Inquiry */
#define AHACOM_SETIMEOUT	0x06	/* Set SCSI selection time out value */
#define AHACOM_BUSON		0x07	/* Set DMA bus on time */
#define AHACOM_BUSOFF		0x08	/* Set DMA bus off time */
#define AHACOM_SPEED		0x09	/* Set DMA transfer speed */
#define AHACOM_INSTALLED	0x0A	/* Return Installed Devices */
#define AHACOM_GETCONFIG	0x0B	/* Return Configuration Data */
#define AHACOM_GETSETUP		0x0D	/* Return Setup Data */
#define AHACOM_EXTBIOS		0x28	/* Return Extended BIOS Info */
#define AHACOM_MBOX_ENABLE	0x29	/* Enable Mailbox Interface */

/* AHA Mailbox Out Codes */
#define AHA_MBOXFREE	0x00	/* Mailbox is Free */
#define AHA_MBOXSTART	0x01	/* Start Command */
#define AHA_MBOXABORT	0x02	/* Abort Command */

/* AHA Mailbox In Codes */
#define AHA_MBOXOK	0x01	/* Command Completed Successfully */
#define AHA_MBOXERR	0x04	/* Command Completed with Error */


/* Basic types */
typedef unsigned char byte;
typedef byte big16[2];	/* 16 bit big-endian values */
typedef byte big24[3];	/* AHA uses 24 bit, big-endian values! */
typedef byte big32[4];	/* Group 1 SCSI commands use 32 bit big-endian values */

/* AHA Mailbox structure */
typedef struct {
  byte status;		/* Command or Status byte */
  big24 ccbptr;		/* pointer to Command Control Block */
} mailbox_t;

/* AHA Command Control Block structure */
typedef struct {
    byte opcode;		/* Operation Code */
#	define CCB_INIT		0x00		/* SCSI Initiator Command */
#	define CCB_TARGET	0x01		/* Target Mode Command */
#	define CCB_SCATTER	0x02	     /* Initiator with scatter/gather */
    byte addrcntl;		/* Address and Direction Control: */
#       define ccb_scid(id)     (((id)<<5)&0xE0) /* SCSI ID field */
#	define CCB_OUTCHECK	0x10		 /* Outbound length check */
#	define CCB_INCHECK	0x08		 /* Inbound length check */
#	define CCB_NOCHECK	0x00		 /* No length check */
#	define ccb_lun(lun)     ((lun)&0x07)	 /* SCSI LUN field */
    byte cmdlen;		/* SCSI Command Length (6 for Group 0) */
    byte senselen;		/* Request/Disable Sense, Allocation Length */
#	define CCB_SENSEREQ	0x0E		/* Request Sense, 14 bytes */
#	define CCB_SENSEOFF	0x01		/* Disable Request Sense */
    big24 datalen;		/* Data Length:  3 bytes, big endian */
    big24 dataptr;		/* Data Pointer: 3 bytes, big endian */
    big24 linkptr;		/* Link Pointer: 3 bytes, big endian */
    byte linkid;		/* Command Linking Identifier */
    byte hastat;		/* Host Adapter Status */
#	define HST_TIMEOUT	0x11		/* SCSI selection timeout */
    byte tarstat;		/* Target Device Status */
    byte reserved[2];		/* reserved, set to 0 */
    byte cmd[12];		/* SCSI Command Descriptor Block */
    byte sense[14];		/* SCSI Request Sense Information */
} ccb_t;

#define	AHA_MAXSENSE_SZ		CCB_SENSEREQ



	/* End of one chunk must be as "odd" as the start of the next. */
#define DMA_CHECK(end, start)	((((int) (end) ^ (int) (start)) & 1) == 0)



/* Miscellaneous parameters */
#define SCSI_TIMEOUT	 250	/* SCSI selection timeout (ms), 0 = none */
#define AHA_TIMEOUT	 5	/* max decisec wait for controller reset */

#ifndef MAX_SCSI_CONTROLLERS
#define	MAX_SCSI_CONTROLLERS		1
#endif

#endif /* __AHA154X_H__ */
