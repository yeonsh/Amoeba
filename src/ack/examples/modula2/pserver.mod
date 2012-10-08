(*	@(#)pserver.mod	1.3	94/04/06 11:11:11 *)
(*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 *)

(*$U+ allow underscores*)
MODULE pserver;

FROM Amoeba	IMPORT header, port, bufsize, getreq, putrep, PORTSIZE;
FROM Threads	IMPORT thread_alloc, thread_exit, thread_newthread;
FROM InOut	IMPORT WriteString, WriteLn;
FROM Arguments	IMPORT Argc, Argv;

CONST
    BUFSIZE = 30000;

VAR hdr:     header;
    prvport: port;
    Hello:   ARRAY [0..20] OF CHAR;
    PortBuf: ARRAY [0..PORTSIZE-1] OF CHAR;
    cmd:     ARRAY [0..255] OF CHAR;
    i:       CARDINAL;


PROCEDURE alg0(VAR buf: ARRAY OF CHAR; size: INTEGER);
BEGIN
    buf[size-1]:= buf[size-1];	(*no-op*)
END alg0;

PROCEDURE alg1(VAR buf: ARRAY OF CHAR; size: INTEGER);
VAR i: INTEGER;
BEGIN
    FOR i:= 0 TO size-1 DO
	INC(buf[i]);
    END;
END alg1;

PROCEDURE alg2(VAR buf: ARRAY OF CHAR; size: INTEGER);
VAR i: INTEGER;
BEGIN
    FOR i:= 0 TO size-1 DO
	IF (INTEGER(buf[i]) MOD 2) = 1 THEN
	    buf[i]:= 0C;
	ELSE
	    buf[i]:= Hello[i MOD 11];
	END;
    END;
END alg2;

PROCEDURE server(): INTEGER;
VAR size: bufsize;
    index: INTEGER;
    buf: POINTER TO ARRAY [0..BUFSIZE-1] OF CHAR;
BEGIN
    index:= 0;
    buf:= thread_alloc(index, BUFSIZE);

    REPEAT
	hdr.h_port:= prvport;
	size:= getreq(hdr, buf, BUFSIZE);
	IF size < 0 THEN
	    WriteString(cmd); WriteString(": getreq failed"); WriteLn();
	    thread_exit();
	END;

	IF (0 < hdr.h_command) AND (hdr.h_command <= 3) THEN
	    CASE hdr.h_command OF
	      1:    alg0(buf^, size);
	    | 2:    alg1(buf^, size);
	    | 3:    alg2(buf^, size);
	    END;
	    hdr.h_command:= 0;	(* hdr.h_status := STD_OK, really *)
	    putrep(hdr, buf, size);
	ELSE
	    putrep(hdr, buf, hdr.h_size);
	END;
    UNTIL hdr.h_extra # 0;
    thread_exit();
    RETURN 0; (* although it never gets executed *)
END server;

BEGIN
    Hello:= "hello, world";
    IF (Argc <> 2) OR (Argv(0, cmd) = 0) OR (Argv(1, PortBuf) = 0) THEN
	WriteString("Usage: pserver port"); WriteLn();
    ELSE
	FOR i:= 0 TO PORTSIZE-1 DO
	    prvport[i]:= INTEGER(PortBuf[i]);
        END;
	thread_newthread(server, 512, NIL, 0);
	i:= server();
    END;
END pserver.
