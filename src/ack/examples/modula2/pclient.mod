(*	@(#)pclient.mod	1.3	94/04/06 11:11:02 *)
(*
 * Copyright 1994 Vrije Universiteit, The Netherlands.
 * For full copyright and restrictions on use see the file COPYRIGHT in the
 * top level of the Amoeba distribution.
 *)

(*$U+ allow underscores*)
MODULE pclient;

FROM SYSTEM	IMPORT	ADR;
FROM Amoeba	IMPORT	header, port, uint16, bufsize, PORTSIZE,
			trans, timeout, priv2pub;
FROM InOut	IMPORT	WriteString, WriteLn, WriteInt;
FROM Arguments	IMPORT	Argv, Argc;
FROM Strings	IMPORT	CompareStr;
FROM Tod	IMPORT	tod_gettime;	(* time of day server for timing *)
FROM Unix	IMPORT	exit;

CONST
    BUFSIZE = 30000;
    STRSIZE = 256;

VAR
    hdr, rethdr:	header;
    prvport, pubport:	port;
    buf, retbuf:	ARRAY [0..BUFSIZE-1] OF CHAR;
    gsize, size, back: 	bufsize;
    extra:		uint16;
    start, finish:	INTEGER;
    i, l, n, cnt:	INTEGER;
    argc, current:	INTEGER;
    ms, tz, dst:	INTEGER;
    reverse:		BOOLEAN;
    argument, prog:	ARRAY [0..STRSIZE-1] OF CHAR;

PROCEDURE GetInt(str: ARRAY OF CHAR; len: INTEGER): INTEGER;
(* Apparently there is no standard string->int conversion routine. *)
VAR    i, res: INTEGER;
BEGIN
    res:= 0;
    i:= 0;
    WHILE (i < len-1) & ('0' <= str[i]) & (str[i] <= '9') DO
	res:= 10*res + INTEGER(ORD(str[i]) - ORD('0'));
	INC(i);
    END;
    RETURN(res);
END GetInt;

BEGIN
    buf:= "Unimportant buffer contents ...";
    retbuf:= "Ditto";

    l:= Argv(0, prog);
    argc:= Argc;
    IF (Argc > 1) AND (Argv(1, argument) # 0) AND
       (CompareStr(argument, "-r") = 0)
    THEN
	reverse:= TRUE;
	current:= 2;
	DEC(argc);
    ELSE
	current:= 1;
	reverse:= FALSE;
    END;

    IF argc # 4 THEN
	WriteString("Usage: "); WriteString(prog);
	WriteString(" [ -r ] port size cnt"); WriteLn();
	exit(1);
    END;

    (* process port argument *)
    l:= Argv(current, argument); INC(current);
    FOR i:= 0 TO PORTSIZE-1 DO
	IF i < l THEN
	    prvport[i]:= INTEGER(argument[i]);
	ELSE
	    prvport[i]:= 0;
	END;
    END;
    priv2pub(prvport, pubport);

    (* process size argument *)
    l:= Argv(current, argument); INC(current);
    size:= GetInt(argument, l);
    gsize:= size;

    (* process cnt argument *)
    l:= Argv(current, argument); INC(current);
    cnt:= GetInt(argument, l);
    IF cnt = 0 THEN
	extra:= 1;			(* tell pserver to exit *)
	n:= 2;				(* it has two threads *)
    ELSE
	extra:= 0;
	n:= cnt;
    END;

    IF reverse THEN			(* let server send data instead *)
	back:= size; size:= 0;
    ELSE
	back:= 0;
    END;

    l:= timeout(5000);
    tod_gettime(start, ms, tz, dst);	(* get start time *)

    FOR i:= 1 TO n DO
	hdr.h_port:= pubport;
	hdr.h_command:= 1;
	hdr.h_extra:= extra;
	hdr.h_size:= back;
	IF (trans(hdr, ADR(buf[0]), size,
		  rethdr, ADR(retbuf[0]), BUFSIZE) < 0) THEN
	    WriteString(prog); WriteString("trans failed"); WriteLn();
	    exit(1);
	END;
	(* Note: h_command is a status when the header gets back *)
	IF rethdr.h_command <> 0 THEN
	    WriteString(prog); WriteString(": non-zero status ");
	    WriteInt(INTEGER(rethdr.h_command), 3); WriteLn();
	    exit(1);
	END;
    END;

    IF cnt = 0 THEN exit(0); END;	(* pserver has exited, so do we *)

    tod_gettime(finish, ms, tz, dst);
    DEC(finish, start);			(* get the time we are running *)

    (* print statistics *)
    WriteString("transfered "); WriteInt(INTEGER(gsize) * cnt, 12);
    WriteString(" bytes in "); WriteInt(finish, 8);
    WriteString(" seconds"); WriteLn();

    IF finish > 0 THEN
	WriteString("transfer rate = ");
	WriteInt(INTEGER(gsize) * cnt DIV finish, 8);
	WriteString(" bytes/second"); WriteLn();
    END;

    WriteString("response time = "); WriteInt(1000000 * finish DIV cnt, 8);
    WriteString(" microsecs"); WriteLn();
END pclient.
