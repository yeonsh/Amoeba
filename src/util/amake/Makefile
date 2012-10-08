#	
#	@(#)Makefile	1.2	94/04/07 14:45:47
#
# Copyright 1994 Vrije Universiteit, The Netherlands.
# For full copyright and restrictions on use see the file COPYRIGHT in the
# top level of the Amoeba distribution.

#
# For bootstrapping purposes: Makefile for amake.
#

SRC_ROOT = <SROOT>

##########

AMAKE_SRC_DIR = $(SRC_ROOT)/util/amake
AMAKE_LIB =     /usr/local/lib/amake
MKDEP =         cc -M
LINT =          lint
SHAR =          shar
TAR =           tar
CC =            cc
LD =            $(CC)
LFLAGS =
HEAD =
FIRSTLIBES =
LASTLIBES =

MAKEFILE =      Makefile # this file

DEBUG =         -DDBUG_OFF -DNDEBUG
CDEF =
COPT =          -O
PROF =

INCLUDES =      -I$(AMAKE_SRC_DIR) -I. 
COMMON =        $(INCLUDES) $(DEBUG) $(CDEF)

CFLAGS =        $(COMMON) $(COPT) $(PROF) $(CMISC)

PFLAGS =        $(COMMON) -egv -p0
TCOVFLAGS =     -a
LDFLAGS =       $(PROF) $(LFLAGS)
LLOPT=          -v

CPP_PREDEF =    -Dsun=1 -Dunix=1
LINTFLAGS =     $(INCLUDES) $(COMMON) $(CPP_PREDEF) -DLINT -nuxz

##########


#
# Amake source description.
#
# PWD has to be set to the name of directory where the sources are.
# Here we assume this Makefile is included, and AMAKE_SRC_DIR has been set.
#
PWD = $(AMAKE_SRC_DIR)

#
# Header source files.
#

H_SRC =				\
	$(PWD)/Lpars.h		\
	$(PWD)/buffer.h		\
	$(PWD)/check.h		\
	$(PWD)/classes.h	\
	$(PWD)/conversion.h	\
	$(PWD)/dbug.h		\
	$(PWD)/declare.h	\
	$(PWD)/dump.h		\
	$(PWD)/error.h		\
	$(PWD)/eval.h		\
	$(PWD)/execute.h	\
	$(PWD)/expr.h		\
	$(PWD)/getoptx.h	\
	$(PWD)/idf.h		\
	$(PWD)/input.h		\
	$(PWD)/lexan.h		\
	$(PWD)/main.h		\
	$(PWD)/os.h		\
	$(PWD)/parser.h		\
	$(PWD)/statefile.h	\
	$(PWD)/symbol2str.h	\
	$(PWD)/tokenname.h	\
				\
	$(PWD)/global.h		\
	$(PWD)/systypes.h	\
	$(PWD)/struct.def	\
	$(PWD)/typedefs.h	\
				\
	$(PWD)/assignment.h	\
	$(PWD)/builtin.h	\
	$(PWD)/caching.h	\
	$(PWD)/cluster.h	\
	$(PWD)/derive.h		\
	$(PWD)/docmd.h		\
	$(PWD)/generic.h	\
	$(PWD)/invoke.h		\
	$(PWD)/objects.h	\
	$(PWD)/scope.h		\
	$(PWD)/slist.h		\
	$(PWD)/tools.h		\
	$(PWD)/type.h

H_ALL = $(H_SRC) $(H_FROM_HH)

#
# C sources files, some of them are generated.
#

C_SRC =				\
	$(PWD)/alloc.c		\
	$(PWD)/assignment.c	\
	$(PWD)/buffer.c		\
	$(PWD)/builtin.c	\
	$(PWD)/caching.c	\
	$(PWD)/check.c		\
	$(PWD)/cluster.c	\
	$(PWD)/conversion.c	\
	$(PWD)/dbug.c		\
	$(PWD)/declare.c	\
	$(PWD)/dump.c		\
	$(PWD)/error.c		\
	$(PWD)/eval.c		\
	$(PWD)/expr.c		\
	$(PWD)/idf.c		\
	$(PWD)/input.c		\
	$(PWD)/invoke.c		\
	$(PWD)/generic.c	\
	$(PWD)/getoptx.c	\
	$(PWD)/lexan.c		\
	$(PWD)/main.c		\
	$(PWD)/os.c		\
	$(PWD)/scope.c 		\
	$(PWD)/slist.c		\
	$(PWD)/statefile.c	\
	$(PWD)/symbol2str.c	\
	$(PWD)/tokenfile.c	\
	$(PWD)/tools.c		\
	$(PWD)/type.c		\
	$(PWD)/classes.c	\
	$(PWD)/derive.c		\
	$(PWD)/docmd.c		\
	$(PWD)/execute.c	\
	$(PWD)/objects.c	\
	$(PWD)/next.c		\
	$(PWD)/tokenname.c

C_ALL =	$(C_SRC)

OBJ =				\
	Lpars.o			\
	alloc.o			\
	assignment.o		\
	buffer.o		\
	builtin.o		\
	caching.o		\
	check.o			\
	classes.o		\
	cluster.o		\
	conversion.o		\
	dbug.o			\
	declare.o		\
	derive.o		\
	docmd.o			\
	dump.o			\
	error.o			\
	eval.o			\
	execute.o		\
	expr.o			\
	generic.o		\
	getoptx.o		\
	idf.o			\
	input.o			\
	invoke.o		\
	lexan.o			\
	main.o			\
	next.o			\
	objects.o		\
	os.o			\
	parser.o		\
	scope.o			\
	slist.o			\
	statefile.o		\
	symbol2str.o		\
	tokenname.o		\
	tokenfile.o		\
	tools.o			\
	type.o


MAKEFILES =	$(PWD)/Makefile
AMAKEFILES =	$(PWD)/Amakefile $(PWD)/Amake.srclist

#
# Accumulate all sources and shellscript for archiving 
#
INP_MODULE =	$(PWD)/inp_pkg.body $(PWD)/inp_pkg.spec
THE_SOURCE =	$(C_SRC) $(H_SRC) $(G_SRC) $(INP_MODULE)
THE_REST =	$(MAKEFILES) $(AMAKEFILES) $(PWD)/README


# 			M a k e f i l e . r u l e s

amake: $(OBJ)
	$(LD) -o $@ $(LDFLAGS)		\
	$(HEAD)				\
	$(OBJ)

# Explicit rules to compile sources from an other directory.
# Necessary when you don't have a Make with pattern matching rules.

Lpars.o:	$(PWD)/Lpars.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/Lpars.c
alloc.o:	$(PWD)/alloc.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/alloc.c
assignment.o:	$(PWD)/assignment.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/assignment.c
buffer.o:	$(PWD)/buffer.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/buffer.c
builtin.o:	$(PWD)/builtin.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/builtin.c
caching.o:	$(PWD)/caching.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/caching.c
check.o:	$(PWD)/check.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/check.c
classes.o:	$(PWD)/classes.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/classes.c
cluster.o:	$(PWD)/cluster.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/cluster.c
conversion.o:	$(PWD)/conversion.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/conversion.c
declare.o:	$(PWD)/declare.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/declare.c
derive.o:	$(PWD)/derive.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/derive.c
dbug.o:		$(PWD)/dbug.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/dbug.c
docmd.o:	$(PWD)/docmd.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/docmd.c
dump.o:		$(PWD)/dump.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/dump.c
error.o:	$(PWD)/error.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/error.c
eval.o:		$(PWD)/eval.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/eval.c
execute.o:	$(PWD)/execute.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/execute.c
expr.o:		$(PWD)/expr.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/expr.c
generic.o:	$(PWD)/generic.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/generic.c
getoptx.o:	$(PWD)/getoptx.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/getoptx.c
idf.o:		$(PWD)/idf.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/idf.c
input.o:	$(PWD)/input.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/input.c
invoke.o:	$(PWD)/invoke.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/invoke.c
lexan.o:	$(PWD)/lexan.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/lexan.c
main.o:		$(PWD)/main.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/main.c
next.o:		$(PWD)/next.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/next.c
objects.o:	$(PWD)/objects.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/objects.c
os.o:		$(PWD)/os.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/os.c
parser.o:	$(PWD)/parser.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/parser.c
scope.o:	$(PWD)/scope.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/scope.c
slist.o:	$(PWD)/slist.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/slist.c
statefile.o:	$(PWD)/statefile.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/statefile.c
structs.o:	$(PWD)/structs.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/structs.c
symbol2str.o:	$(PWD)/symbol2str.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/symbol2str.c
tokenname.o:	$(PWD)/tokenname.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/tokenname.c
tokenfile.o:	$(PWD)/tokenfile.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/tokenfile.c
tools.o:	$(PWD)/tools.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/tools.c
type.o:		$(PWD)/type.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c $(PWD)/type.c


# 
# Make dependency list generation (put in Makefile.dep).
#

depend:
	$(MKDEP) $(INCLUDES) $(DEBUG) $(C_ALL) | \
	sed -e 's/\.c:/\.o:/' > Makefile.dep

#
# tags and lint
# 

tags:	$(C_ALL)
	$(TAGS) $(C_ALL)

lint:	$(C_ALL)
	$(LINT) $(LINTFLAGS) $(C_ALL)

#
# Archiving entries, use your favorit one.
#

shar:
	$(SHAR) $(THE_SOURCE) $(THE_REST) > amake.shar


tar:
	$(TAR) cf - $(THE_SOURCE) $(THE_REST) > amake.tar

#
# Cleanup. Veryclean only leaves real sources alone.
#

clean:
	rm -f amake $(OBJ)

veryclean:
	rm -f amake $(OBJ) core tags amake.tar amake.shar

#
#	Source/objectfile dependencies
#

d = $(PWD)

alloc.o:	$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h
alloc.o:	$d/error.h $d/standlib.h

assignment.o:	$d/global.h $d/typedefs.h $d/struct.def
assignment.o:	$d/standlib.h $d/alloc.h $d/dbug.h $d/global.h $d/idf.h
assignment.o:	$d/idf_pkg.spec $d/expr.h $d/error.h $d/eval.h $d/lexan.h
assignment.o:	$d/f_info.h $d/execute.h $d/slist.h $d/type.h $d/builtin.h
assignment.o:	$d/buffer.h $d/conversion.h $d/scope.h $d/statefile.h
assignment.o:	$d/caching.h $d/assignment.h $d/input.h $d/inp_pkg.spec

buffer.o:	$d/global.h $d/typedefs.h $d/struct.def $d/standlib.h
buffer.o:	$d/alloc.h $d/buffer.h

builtin.o:	$d/global.h $d/typedefs.h $d/struct.def $d/standlib.h
builtin.o:	$d/alloc.h $d/idf.h $d/idf_pkg.spec $d/expr.h $d/type.h
builtin.o:	$d/slist.h $d/scope.h $d/Lpars.h $d/dbug.h $d/global.h
builtin.o:	$d/objects.h $d/eval.h $d/execute.h $d/error.h $d/conversion.h
builtin.o:	$d/assignment.h $d/cluster.h $d/main.h $d/buffer.h $d/invoke.h
builtin.o:	$d/lexan.h $d/f_info.h $d/os.h $d/builtin.h

caching.o:	$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h
caching.o:	$d/idf.h $d/idf_pkg.spec $d/main.h $d/slist.h $d/objects.h
caching.o:	$d/expr.h $d/scope.h $d/type.h $d/builtin.h $d/dbug.h
caching.o:	$d/global.h $d/Lpars.h $d/lexan.h $d/f_info.h $d/dump.h
caching.o:	$d/symbol2str.h $d/execute.h $d/assignment.h $d/error.h
caching.o:	$d/conversion.h $d/cluster.h $d/eval.h $d/invoke.h $d/os.h
caching.o:	$d/tools.h $d/statefile.h $d/caching.h

check.o:	$d/global.h $d/typedefs.h $d/struct.def $d/idf.h
check.o: 	$d/idf_pkg.spec $d/type.h $d/structdef.h $d/symbol2str.h
check.o:	$d/Lpars.h $d/error.h $d/expr.h $d/scope.h $d/dbug.h
check.o:	$d/global.h $d/slist.h $d/builtin.h $d/objects.h $d/check.h

cluster.o:	$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h
cluster.o:	$d/Lpars.h $d/idf.h $d/idf_pkg.spec $d/dbug.h $d/global.h
cluster.o:	$d/error.h $d/slist.h $d/structdef.h $d/eval.h $d/expr.h
cluster.o:	$d/objects.h $d/execute.h $d/type.h $d/parser.h $d/conversion.h
cluster.o:	$d/tools.h $d/scope.h $d/invoke.h $d/main.h $d/caching.h
cluster.o:	$d/declare.h $d/os.h $d/assignment.h $d/cluster.h
cluster.o:	$d/statefile.h $d/classes.h

conversion.o:	$d/global.h $d/typedefs.h $d/struct.def
conversion.o:	$d/standlib.h $d/alloc.h $d/error.h $d/type.h $d/structdef.h
conversion.o:	$d/idf.h $d/idf_pkg.spec $d/expr.h $d/objects.h $d/builtin.h
conversion.o:	$d/Lpars.h $d/slist.h $d/symbol2str.h $d/dbug.h $d/global.h
conversion.o:	$d/dump.h $d/statefile.h $d/conversion.h

declare.o:	$d/dbug.h $d/global.h $d/typedefs.h $d/struct.def
declare.o:	$d/global.h $d/expr.h $d/eval.h $d/execute.h $d/declare.h
declare.o:	$d/lexan.h $d/f_info.h $d/slist.h $d/structdef.h $d/objects.h
declare.o:	$d/conversion.h $d/Lpars.h $d/type.h

dump.o:		$d/global.h $d/typedefs.h $d/struct.def $d/idf.h
dump.o:		$d/idf_pkg.spec $d/Lpars.h $d/lexan.h $d/f_info.h $d/dbug.h
dump.o:		$d/global.h $d/error.h $d/dump.h

error.o:	$d/global.h $d/typedefs.h $d/struct.def
error.o:	$d/standlib.h $d/structdef.h $d/idf.h $d/idf_pkg.spec
error.o:	$d/type.h $d/scope.h $d/lexan.h $d/f_info.h $d/main.h
error.o:	$d/statefile.h $d/docmd.h $d/execute.h $d/expr.h $d/error.h

eval.o:		$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h
eval.o:		$d/idf.h $d/idf_pkg.spec $d/type.h $d/structdef.h $d/expr.h
eval.o:		$d/eval.h $d/error.h $d/dbug.h $d/global.h $d/Lpars.h
eval.o:		$d/parser.h $d/scope.h $d/slist.h $d/builtin.h $d/execute.h
eval.o:		$d/conversion.h $d/assignment.h $d/invoke.h $d/caching.h
eval.o:		$d/statefile.h $d/derive.h $d/objects.h

expr.o:		$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h
expr.o:		$d/dump.h $d/idf.h $d/idf_pkg.spec $d/Lpars.h $d/symbol2str.h
expr.o:		$d/structdef.h $d/error.h $d/type.h $d/dbug.h $d/global.h
expr.o:		$d/objects.h $d/slist.h $d/scope.h $d/tools.h $d/assignment.h
expr.o:		$d/generic.h $d/main.h $d/expr.h $d/builtin.h

idf.o:		$d/global.h $d/typedefs.h $d/struct.def $d/idf.h
idf.o:		$d/idf_pkg.spec $d/idf_pkg.body $d/alloc.h

input.o:	$d/input.c $d/global.h $d/typedefs.h $d/struct.def $d/f_info.h
input.o:	$d/input.h $d/inp_pkg.spec $d/inp_pkg.body $d/standlib.h
input.o:	$d/alloc.h

invoke.o:	$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h $d/Lpars.h
invoke.o:	$d/idf.h $d/idf_pkg.spec $d/dbug.h $d/global.h $d/error.h
invoke.o:	$d/slist.h $d/structdef.h $d/eval.h $d/expr.h $d/objects.h
invoke.o:	$d/execute.h $d/type.h $d/conversion.h $d/tools.h $d/scope.h
invoke.o:	$d/caching.h $d/builtin.h $d/cluster.h $d/main.h $d/os.h
invoke.o:	$d/invoke.h

generic.o:	$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h $d/idf.h
generic.o:	$d/idf_pkg.spec $d/scope.h $d/structdef.h $d/error.h $d/input.h
generic.o:	$d/inp_pkg.spec $d/lexan.h $d/f_info.h $d/expr.h $d/slist.h
generic.o:	$d/dbug.h $d/global.h $d/dump.h $d/buffer.h $d/assignment.h
generic.o:	$d/execute.h $d/generic.h

getoptx.o:	$d/getoptx.h

lexan.o:	$d/global.h $d/typedefs.h $d/struct.def $d/standlib.h
lexan.o:	$d/alloc.h $d/idf.h $d/idf_pkg.spec $d/scope.h $d/structdef.h
lexan.o:	$d/Lpars.h $d/parser.h $d/symbol2str.h $d/error.h $d/dbug.h
lexan.o:	$d/global.h $d/objects.h $d/slist.h $d/main.h $d/input.h
lexan.o:	$d/inp_pkg.spec $d/expr.h $d/assignment.h $d/tokenname.h
lexan.o:	$d/os.h $d/lexan.h $d/f_info.h

main.o:		$d/global.h $d/typedefs.h $d/struct.def $d/standlib.h
main.o:		$d/alloc.h $d/dbug.h $d/global.h $d/getoptx.h $d/idf.h
main.o:		$d/idf_pkg.spec $d/scope.h $d/structdef.h $d/error.h
main.o:		$d/lexan.h $d/f_info.h $d/symbol2str.h $d/type.h $d/expr.h
main.o:		$d/eval.h $d/check.h $d/objects.h $d/builtin.h $d/execute.h
main.o:		$d/docmd.h $d/declare.h $d/assignment.h $d/tokenname.h
main.o:		$d/derive.h $d/cluster.h $d/caching.h $d/invoke.h $d/dump.h
main.o:		$d/slist.h $d/statefile.h $d/conversion.h $d/os.h $d/tools.h
main.o:		$d/generic.h $d/main.h $d/classes.h

os.o:		$d/systypes.h $d/standlib.h $d/unistand.h $d/global.h
os.o:		$d/typedefs.h $d/struct.def $d/structdef.h $d/alloc.h $d/idf.h
os.o:		$d/idf_pkg.spec $d/error.h $d/expr.h $d/scope.h $d/objects.h
os.o:		$d/dbug.h $d/global.h $d/statefile.h $d/buffer.h $d/docmd.h
os.o:		$d/main.h $d/caching.h $d/slist.h $d/type.h $d/conversion.h
os.o:		$d/builtin.h $d/os.h

scope.o:	$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h $d/dump.h
scope.o:	$d/idf.h $d/idf_pkg.spec $d/type.h $d/structdef.h $d/Lpars.h
scope.o:	$d/error.h $d/main.h $d/slist.h $d/cluster.h $d/dbug.h
scope.o:	$d/global.h $d/assignment.h $d/tools.h $d/generic.h $d/expr.h
scope.o:	$d/builtin.h $d/scope.h

slist.o:	$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h $d/slist.h
slist.o:	$d/structdef.h

statefile.o:	$d/global.h $d/typedefs.h $d/struct.def $d/unistand.h
statefile.o:	$d/alloc.h $d/idf.h $d/idf_pkg.spec $d/Lpars.h $d/error.h
statefile.o:	$d/objects.h $d/structdef.h $d/expr.h $d/type.h $d/caching.h
statefile.o:	$d/dump.h $d/dbug.h $d/global.h $d/builtin.h $d/scope.h
statefile.o:	$d/slist.h $d/assignment.h $d/main.h $d/cluster.h $d/lexan.h
statefile.o:	$d/f_info.h $d/invoke.h $d/execute.h $d/symbol2str.h $d/os.h
statefile.o:	$d/conversion.h $d/docmd.h $d/statefile.h

symbol2str.o:	$d/Lpars.h

tokenfile.o:	$d/Lpars.h

tools.o:	$d/global.h $d/typedefs.h $d/struct.def $d/slist.h
tools.o:	$d/structdef.h $d/type.h $d/expr.h $d/scope.h $d/tools.h
tools.o:	$d/classes.h

type.o:		$d/dbug.h $d/global.h $d/typedefs.h $d/struct.def $d/global.h
type.o:		$d/alloc.h $d/error.h $d/expr.h $d/type.h

classes.o:	$d/global.h $d/typedefs.h $d/struct.def $d/alloc.h $d/idf.h
classes.o:	$d/idf_pkg.spec $d/error.h $d/slist.h $d/structdef.h $d/expr.h
classes.o:	$d/dbug.h $d/global.h $d/type.h $d/eval.h $d/tools.h $d/scope.h
classes.o:	$d/Lpars.h $d/objects.h $d/cluster.h $d/caching.h $d/dump.h
classes.o:	$d/classes.h

derive.o:	$d/global.h $d/typedefs.h $d/struct.def $d/structdef.h
derive.o:	$d/alloc.h $d/idf.h $d/idf_pkg.spec $d/expr.h $d/slist.h
derive.o:	$d/objects.h $d/builtin.h $d/eval.h $d/scope.h $d/error.h
derive.o:	$d/dbug.h $d/global.h $d/type.h $d/execute.h $d/Lpars.h
derive.o:	$d/main.h $d/derive.h

docmd.o:	$d/standlib.h $d/global.h $d/typedefs.h $d/struct.def
docmd.o:	$d/alloc.h $d/dbug.h $d/global.h $d/slist.h $d/structdef.h
docmd.o:	$d/error.h $d/execute.h $d/expr.h $d/builtin.h $d/objects.h
docmd.o:	$d/invoke.h $d/buffer.h $d/main.h $d/os.h $d/docmd.h $d/Lpars.h

execute.o:	$d/global.h $d/typedefs.h $d/struct.def $d/structdef.h
execute.o:	$d/alloc.h $d/slist.h $d/docmd.h $d/expr.h $d/builtin.h
execute.o:	$d/idf.h $d/idf_pkg.spec $d/dbug.h $d/global.h $d/error.h
execute.o:	$d/eval.h $d/type.h $d/scope.h $d/derive.h $d/Lpars.h
execute.o:	$d/objects.h $d/main.h $d/os.h $d/execute.h $d/structdef.h

objects.o:	$d/global.h $d/typedefs.h $d/struct.def $d/standlib.h
objects.o:	$d/alloc.h $d/idf.h $d/idf_pkg.spec $d/expr.h $d/Lpars.h
objects.o:	$d/scope.h $d/derive.h $d/slist.h $d/dump.h $d/dbug.h
objects.o:	$d/global.h $d/execute.h $d/eval.h $d/type.h $d/error.h
objects.o:	$d/main.h $d/conversion.h $d/os.h $d/objects.h

tokenname.o:	$d/global.h $d/typedefs.h $d/struct.def $d/Lpars.h $d/idf.h
tokenname.o:	$d/idf_pkg.spec

