#	
#	@(#)Makefile	1.2	94/04/05 17:26:10
#
# Copyright 1994 Vrije Universiteit, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Amoeba distribution.

CONF_ROOT = <CROOT>
SRC_ROOT = <SROOT>

CC = cc
ALL =  mkproto
SRCS = $(SRC_ROOT)/util/mkproto/mkproto.c
HFILES = $(SRC_ROOT)/util/mkproto/mkproto.h

mkproto : $(SRCS) $(HFILES)
	$(CC) -O -o $@ $(SRCS)
clean:
	rm -f *.o

realclean: clean
	rm -f $(ALL) report core
