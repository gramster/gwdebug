# Choose whether you want local heap, debugging, neither or both.
# If you change either of these be sure to delete all object files
# before the next make.

DEBUG=-DGW_DEBUG
#DEBUG=
#HEAP=-DLOCAL_HEAP -DCHK_USE
#HEAP=-DLOCAL_HEAP
HEAP=
MODEL=-ml

###########################################################
# Don't change below here, except to switch between DOS/UNIX
# or to change the log file setting.

DOSOSUF=.obj
UNIXOSUF=.o
DOSXSUF=.exe
UNIXXSUF=
DOSCFLAGS=-v -ls $(DEBUG) $(HEAP) $(MODEL)
UNIXCFLAGS=-g $(DEBUG) $(HEAP)
DOSLOG="gwtest.log"
UNIXLOG="\"gwtest.log\""

# UNIX OPTIONS

#CC=cc
#CFLAGS=$(UNIXCFLAGS) -ogwtest
#LOG=$(UNIXLOG)
#OSUF=$(UNIXOSUF)
#XSUF=$(UNIXXSUF)
#ZIP=zip

# DOS OPTIONS

CC=bcc
CFLAGS=$(DOSCFLAGS)
LOG=$(DOSLOG)
OSUF=$(DOSOSUF)
XSUF=$(DOSXSUF)
ZIP=pkzip

all: gwtest$(XSUF) testheap$(XSUF)

gwtest$(XSUF): gwtest$(OSUF) gwdebug$(OSUF) heap$(OSUF)
	$(CC) $(CFLAGS) gwtest$(OSUF) gwdebug$(OSUF) heap$(OSUF)

testheap$(XSUF): testheap$(OSUF) gwdebug$(OSUF) heap$(OSUF)
	$(CC) $(CFLAGS) testheap$(OSUF) gwdebug$(OSUF) heap$(OSUF)

gwtest$(OSUF): gwtest.c gwdebug.h heap.h
	$(CC) -c $(CFLAGS) gwtest.c

testheap$(OSUF): testheap.c gwdebug.h heap.h
	$(CC) -c $(CFLAGS) testheap.c

gwdebug$(OSUF): gwdebug.c gwdebug.h heap.h
	$(CC) -c $(CFLAGS) -DDEBUG_LOG=$(LOG) gwdebug.c

gwheap$(OSUF): gwheap.c heap.h
	$(CC) -c $(CFLAGS) gwheap.c

zip:
	$(ZIP) -ur gwdebug gwdebug.c gwdebug.h gwtest.c heap.c heap.h testheap.c Makefile trace.c


