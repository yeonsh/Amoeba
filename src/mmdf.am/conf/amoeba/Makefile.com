#	
#	@(#)Makefile.com	1.4	96/03/07 14:07:46
#

#
#	Common Sub-Makefile for the MMDF System
#

# The following are edited by the buildmmdf script at build time so that
# we can cross-compile properly
#
SRCROOT		= <SRCROOT>
CONFROOT	= <CONFROOT>
ARCH		= <ARCH>
COMPILERSET	= <COMPILERSET>
AR		= <AR>
ARFLAGS		= <ARFLAGS>
LD		= <LD>
LDFLAGS 	= <LDFLAGS>
CC		= <CC>
CFLAGSEXTRA	= <CFLAGS>
AINSTALL	= <AINSTALL>
MKDEPFLAGS	= <MKDEPFLAGS>
HEAD		= <HEAD>

AINSTALLFLAGS	= -m $(ARCH)

#
HOST		= amoeba
SYSTEM		= am
MMPREF		=
LIBDIR		= /super/module/mmdf
CHANDIR		= /super/module/mmdf/chans
TBLDIR		= /super/module/mmdf/table
BINDIR		= /super/util
RCVDIR		= /super/module/mmdf/rcvmail

#
#  Defines used during installation
#
CHOWN		= /etc/chown
INSTALL         = /usr/bin/install
MMDFLOGIN	= mmdf
ROOTLOGIN	= Daemon
PGMPROT 	= 755

#
#  Configuration Defines
#
#  See the document "Installing and Operating MMDF II" for descriptions
#  of the available CONFIGDEFS options.
#  (The paths below are relative from the individual source directories.)
#
CONFIGDEFS	= -DNODOMLIT -DNODIAL -DV4_2BSD \
		  -DAMOEBA -DTCPIPSERV=\"/super/cap/ipsvr/tcp\"
CFLAGS		= $(CFLAGSEXTRA) \
		-D$(ARCH) \
		-I$(SRCROOT)/h/posix \
		-I$(SRCROOT)/h \
		-I$(SRCROOT)/h/posix/machdep/$(ARCH) \
		-I$(SRCROOT)/h/ajax \
		-I$(SRCROOT)/h/server \
		-I$(SRCROOT)/h/class \
		-I$(SRCROOT)/h/machdep/arch/$(ARCH) \
		-I$(SRCROOT)/h/toolset/$(COMPILERSET) \
		-I../../h \
		-I/usr/include \
		-Drindex=strrchr \
		-Dindex=strchr \
		$(CONFIGDEFS)
MMDFLIBS 	= ../../lib/libmmdf.a
AMLIBDIR	= $(CONFROOT)/$(ARCH).$(COMPILERSET)/lib
SYSLIBS		= $(AMLIBDIR)/ajax/libajax.a \
		  $(AMLIBDIR)/amoeba/libamoeba.a \
		  #-ldbm ../../libz/libz.a ../../libndir/libndir.a -lresolv

LINT		= lint
LFLAGS		= -bxah \
		  -I$(SRCROOT)/h/posix \
		  -I$(SRCROOT)/h \
		  -I$(SRCROOT)/h/posix/machdep/$(ARCH)  \
		  -I$(SRCROOT)/h/ajax \
		  -I$(SRCROOT)/h/server \
		  -I$(SRCROOT)/h/class \
		  -I../../h $(CONFIGDEFS)
LLIBS		= ../../lib/llib-lmmdf.ln


#  Specify either ch_tbdbm (for DBM based tables) or ch_tbseq for
#  sequential IO based tables.
CH_TB	= ch_tbseq

#  Specify one of the nameserver modules or the fake module
#  if you do not intend to support nameservers (4.2, fake)
TB_NS	= fake

#  Specify tai_???.o and lk_lock???.o
LOCALUTIL = tai_file.o lk_lock.o


default:	real-default

.c.o:
	$(CC) $(CFLAGS) -c $<

.SUFFIXES: .o .c

#
#  special case dependencies
#
../../h/mmdf.h:	../../h/conf.h
	-touch ../../h/mmdf.h
.PRECIOUS: ../../h/mmdf.h

#
#  #include dependencies
#
#  Two versions are supplied.  One for sites with cc -M (4.3BSD)
#  and one for those that don't have it.  Comment out the one
#  you do not want.

#  This one is for sites without cc -M
#depend:
#	cat </dev/null >x.c
#	for i in $(MODULES); do \
#		(echo $$i.o: $$i.c >>makedep; \
#		/bin/grep '^#[ 	]*include' x.c $$i.c | sed \
#			-e 's,c:[^"]*"\./\([^"]*\)".*,o: \1,' \
#			-e 's,c:[^"]*"/\([^"]*\)".*,o: /\1,' \
#			-e 's,c:[^"]*"\([^"]*\)".*,o: ../../h/\1,' \
#			-e 's,c:[^<]*<\(.*\)>.*,o: /usr/include/\1,' \
#			>>makedep); done
#	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
#	echo '$$r makedep' >>eddep
#	echo 'w' >>eddep
#	cp Makefile.real Makefile.bak
#	ed - Makefile.real < eddep
#	rm eddep makedep x.c
#	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile.real
#	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile.real
#	echo '# see make depend above' >> Makefile.real

#  This one id for sites with cc -M
depend:
	( for i in ${MODULES} ; do \
		${CC} ${MKDEPFLAGS} ${CFLAGS} $$i.c ; done ) | \
	awk ' { if ($$1 != prev) { print rec; rec = $$0; prev = $$1; } \
		else { if (length(rec $$2) > 78) { print rec; rec = $$0; } \
		       else rec = rec " " $$2 } } \
	      END { print rec } ' > makedep
	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w Makefile.real' >>eddep
	rm -f Makefile.bak
	mv Makefile.real Makefile.bak
	ed - Makefile.bak < eddep
	rm eddep makedep
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile.real
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile.real
	echo '# see make depend above' >> Makefile.real
