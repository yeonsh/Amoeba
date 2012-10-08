#	
#	@(#)Makefile	1.1	91/04/09 17:04:35
#
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
