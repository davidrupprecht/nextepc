## Process this file with automake to produce Makefile.in.

bin_PROGRAMS = testvolte

testvolte_SOURCES = \
	abts.c abts.h testpacket.h testpacket.c \
	testutil.c testutil.h testapp.h testapp.c \
	pcscf_fd_path.c volte_test.c \
	$(NULL)

testvolte_LDADD = \
    $(top_srcdir)/src/libepc.la \
	$(NULL)

AM_CPPFLAGS = \
    -I$(top_srcdir)/lib/core/include \
    -I$(top_srcdir)/lib \
    -I$(top_srcdir)/src \
    -I$(top_srcdir)/lib/s1ap/asn1c \
	-I$(top_srcdir)/lib/@FREEDIAMETER_DIR@/include \
	@MONGOC_CFLAGS@ \
	$(NULL)

AM_CFLAGS = \
	-Wall -Werror \
	-Wno-unused-function -Wno-unused-variable \
	@OSCPPFLAGS@ \
	$(NULL)

MAINTAINERCLEANFILES = Makefile.in 
MOSTLYCLEANFILES = core *.stackdump
