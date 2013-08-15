
# Also see "include/polarssl/config.h"

# To compile on MinGW: add "-lws2_32" to LDFLAGS or define WINDOWS in your
# environment
#
CFLAGS	+= -I../include -D_FILE_OFFSET_BITS=64 -Wall -W -Wdeclaration-after-statement
OFLAGS	= -O2

ifdef DEBUG
CFLAGS += -g3
endif

# MicroBlaze specific options:
# CFLAGS += -mno-xl-soft-mul -mxl-barrel-shift

# To compile on Plan9:
# CFLAGS += -D_BSD_EXTENSION

# To compile as a shared library:
ifdef SHARED
CFLAGS += -fPIC
endif

SONAME=libpolarssl.so.0

DLEXT=so
# OSX shared library extension:
# DLEXT=dylib

# Windows shared library extension:
ifdef WINDOWS
DLEXT=dll
LDFLAGS += -lws2_32
endif

OBJS=	aes.o		arc4.o		asn1parse.o		\
		asn1write.o base64.o	bignum.o		\
		blowfish.o	camellia.o					\
		certs.o		cipher.o	cipher_wrap.o	\
		ctr_drbg.o	debug.o		des.o			\
		dhm.o		entropy.o	entropy_poll.o	\
		error.o		gcm.o		havege.o		\
		md.o		md_wrap.o	md2.o			\
		md4.o		md5.o		net.o			\
		padlock.o	pbkdf2.o	pem.o			\
		pkcs5.o		pkcs11.o	pkcs12.o		\
		rsa.o		sha1.o		sha2.o			\
		sha4.o		ssl_cache.o	ssl_cli.o		\
		ssl_srv.o								\
		ssl_tls.o	timing.o	version.o		\
		x509parse.o	x509write.o	xtea.o

.SILENT:

ifndef SHARED
all: static
else
all: shared
endif

static: libpolarssl.a

shared: libpolarssl.$(DLEXT)

libpolarssl.a: $(OBJS)
	echo "  AR    $@"
	$(AR) r $@ $(OBJS)
	echo "  RL    $@"
	$(AR) s $@

libpolarssl.so: libpolarssl.a
	echo "  LD    $@"
	$(CC) ${LDFLAGS} -shared -Wl,-soname,$(SONAME) -o $@ $(OBJS)

libpolarssl.dylib: libpolarssl.a
	echo "  LD    $@"
	$(CC) ${LDFLAGS} -dynamiclib -o $@ $(OBJS)

libpolarssl.dll: libpolarssl.a 
	echo "  LD    $@" 
	$(CC) -shared -Wl,-soname,$@ -o $@ $(OBJS) -lws2_32 -lwinmm -lgdi32 

.c.o:
	echo "  CC    $<"
	$(CC) $(CFLAGS) $(OFLAGS) -c $<

clean:
ifndef WINDOWS
	rm -f *.o libpolarssl.*
endif
ifdef WINDOWS
	del /Q /F *.o libpolarssl.*
endif
