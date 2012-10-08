/*	@(#)keyboard.h	1.3	94/04/06 09:20:56 */
/*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 */

/* Keyboard manifest constants */
#define	KBD_IRQ		1	/* keyboard's IRQ level */
#define	KBD_RETRIES	0x1000	/* arbitrary retry limit */

/* Keyboard I/O registers */
#define	KBD_PORTB	0x61	/* 8255 PPI port: keyboard, beeper */
#ifdef ISA
#define	KBD_DATA	0x60	/* keyboard data on a at */
#define	KBD_STATUS	0x64	/* keyboard status on a at */
#elif MCA
#define	KBD_DATA	0x72	/* keyboard data on a ps/2 */
#define	KBD_STATUS	0x68	/* keyboard status on a ps/2 */
#endif

/* keyboard commands/responses */
#define	KBD_RESET	0xFF	/* reset keyboard */
#define KBD_RESEND	0xFE	/* keyboard resend response */
#define	KBD_DIAGFAIL	0xFD	/* diagnostic failure */
#define KBD_NOP		0xFD	/* nop command */
#define KBD_ACK		0xFA	/* acknowledgement byte from keyboard */
#define	KBD_ENABLE	0xF4	/* enable keyboard scanning */
#define	KBD_TYPO	0xF3	/* set keyboard typematic rate */
#define	KBD_SETLEDS	0xED	/* set/reset keyboard leds */
#define	KBD_ECHO	0xEE	/* echo 'EE' data back */
#define	KBD_BAT		0xAA	/* basic assurance test */
#define	KBD_NOBAT	0x2A	/* not basic assurance test */
#define	KBD_OVERRUN	0x00	/* keyboard overrun */

/* keyboard led bits */
#define KBD_LEDCAP	0x04	/* flag bit for cap lock */
#define KBD_LEDNUM	0x02	/* flag bit for num lock */
#define	KBD_LEDSCROLL	0x01	/* flag bit for scroll lock */

/* bits for 8255 programmable peripheral interface */
#define	KBD_SPEAKER	0x03	/* speaker bit */
#define	KBD_KBIT	0x80	/* to ack received character */

/* bits for 8042 */
#define	KBD_RESETBIT	0x01	/* reset processor (write) */
#define	KBD_GATEA20	0x02	/* enable A20 line (write) */
#define	KBD_BUSY	0x02	/* busy status (read) */
#define	KBD_PULSE	0xF0	/* base for commands to pulse output port */

/* some useful scan code limitations */
#define	KBD_SCODE1	71	/* Home on numeric key pad */
#define	KBD_SCODE2	81	/* PgDn on numeric key pad */
#define	KBD_TOPROW	14	/* code below are shifted */

/* some useful keyboard scan codes */
#define	KBD_CONTROL	0x1D	/* control key */
#define	KBD_LSHIFT	0x2A	/* left shift key */
#define	KBD_RSHIFT	0x36	/* right shift key */
#define KBD_PRINT	0x37	/* print key */
#define	KBD_ALT		0x38	/* alt key */
#define	KBD_CAPS	0x3A	/* caps-lock key */
#define	KBD_NUM		0x45	/* num-lock key */
#define	KBD_SCROLL	0x46	/* scroll-lock key */
#define KBD_DELETE	0x53	/* delete key */

