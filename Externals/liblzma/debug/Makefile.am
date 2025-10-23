## SPDX-License-Identifier: 0BSD
## Author: Lasse Collin

EXTRA_DIST = \
	translation.bash

noinst_PROGRAMS = \
	repeat \
	sync_flush \
	full_flush \
	memusage \
	crc32 \
	known_sizes \
	hex2bin \
	testfilegen-arm64

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/common \
	-I$(top_srcdir)/src/liblzma/api

LDADD = $(top_builddir)/src/liblzma/liblzma.la

if COND_GNULIB
LDADD += $(top_builddir)/lib/libgnu.a
endif

LDADD += $(LTLIBINTL)
