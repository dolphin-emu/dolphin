
# Also see "include/mbedtls/config.h"

CFLAGS	?= -O2
WARNING_CFLAGS ?=  -Wall -Wextra -Wformat=2 -Wno-format-nonliteral
LDFLAGS ?=

# Include ../include for public headers and . for private headers.
# Note that . needs to be included explicitly for the sake of library
# files that are not in the /library directory (which currently means
# under /3rdparty).
LOCAL_CFLAGS = $(WARNING_CFLAGS) -I. -I../include -D_FILE_OFFSET_BITS=64
LOCAL_LDFLAGS =

ifdef DEBUG
LOCAL_CFLAGS += -g3
endif

# MicroBlaze specific options:
# CFLAGS += -mno-xl-soft-mul -mxl-barrel-shift

# To compile on Plan9:
# CFLAGS += -D_BSD_EXTENSION

# if were running on Windows build for Windows
ifdef WINDOWS
WINDOWS_BUILD=1
else ifeq ($(shell uname -s),Darwin)
ifeq ($(AR),ar)
APPLE_BUILD ?= 1
endif
endif

# To compile as a shared library:
ifdef SHARED
# all code is position-indep with mingw, avoid warning about useless flag
ifndef WINDOWS_BUILD
LOCAL_CFLAGS += -fPIC -fpic
endif
endif

SOEXT_TLS=so.14
SOEXT_X509=so.1
SOEXT_CRYPTO=so.7

# Set AR_DASH= (empty string) to use an ar implementation that does not accept
# the - prefix for command line options (e.g. llvm-ar)
AR_DASH ?= -

ARFLAGS = $(AR_DASH)src
ifdef APPLE_BUILD
ifneq ($(APPLE_BUILD),0)
ARFLAGS = $(AR_DASH)Src
RLFLAGS = -no_warning_for_no_symbols -c
RL ?= ranlib
endif
endif

DLEXT ?= so
ifdef WINDOWS_BUILD
# Windows shared library extension:
DLEXT = dll
else ifdef APPLE_BUILD
ifneq ($(APPLE_BUILD),0)
# Mac OS X shared library extension:
DLEXT = dylib
endif
endif

OBJS_CRYPTO= \
	     aes.o \
	     aesni.o \
	     arc4.o \
	     aria.o \
	     asn1parse.o \
	     asn1write.o \
	     base64.o \
	     bignum.o \
	     blowfish.o \
	     camellia.o \
	     ccm.o \
	     chacha20.o \
	     chachapoly.o \
	     cipher.o \
	     cipher_wrap.o \
	     cmac.o \
	     constant_time.o \
	     ctr_drbg.o \
	     des.o \
	     dhm.o \
	     ecdh.o \
	     ecdsa.o \
	     ecjpake.o \
	     ecp.o \
	     ecp_curves.o \
	     entropy.o \
	     entropy_poll.o \
	     error.o \
	     gcm.o \
	     havege.o \
	     hkdf.o \
	     hmac_drbg.o \
	     md.o \
	     md2.o \
	     md4.o \
	     md5.o \
	     memory_buffer_alloc.o \
	     mps_reader.o \
	     mps_trace.o \
	     nist_kw.o \
	     oid.o \
	     padlock.o \
	     pem.o \
	     pk.o \
	     pk_wrap.o \
	     pkcs12.o \
	     pkcs5.o \
	     pkparse.o \
	     pkwrite.o \
	     platform.o \
	     platform_util.o \
	     poly1305.o \
	     psa_crypto.o \
	     psa_crypto_aead.o \
	     psa_crypto_cipher.o \
	     psa_crypto_client.o \
	     psa_crypto_driver_wrappers.o \
	     psa_crypto_ecp.o \
	     psa_crypto_hash.o \
	     psa_crypto_mac.o \
	     psa_crypto_rsa.o \
	     psa_crypto_se.o \
	     psa_crypto_slot_management.o \
	     psa_crypto_storage.o \
	     psa_its_file.o \
	     ripemd160.o \
	     rsa.o \
	     rsa_internal.o \
	     sha1.o \
	     sha256.o \
	     sha512.o \
	     threading.o \
	     timing.o \
	     version.o \
	     version_features.o \
	     xtea.o \
	     # This line is intentionally left blank

include ../3rdparty/Makefile.inc
LOCAL_CFLAGS+=$(THIRDPARTY_INCLUDES)
OBJS_CRYPTO+=$(THIRDPARTY_CRYPTO_OBJECTS)

OBJS_X509= \
	   certs.o \
	   pkcs11.o \
	   x509.o \
	   x509_create.o \
	   x509_crl.o \
	   x509_crt.o \
	   x509_csr.o \
	   x509write_crt.o \
	   x509write_csr.o \
	   # This line is intentionally left blank

OBJS_TLS= \
	  debug.o \
	  net_sockets.o \
	  ssl_cache.o \
	  ssl_ciphersuites.o \
	  ssl_cli.o \
	  ssl_cookie.o \
	  ssl_msg.o \
	  ssl_srv.o \
	  ssl_ticket.o \
	  ssl_tls.o \
	  ssl_tls13_keys.o \
	  # This line is intentionally left blank

.SILENT:

.PHONY: all static shared clean

ifndef SHARED
all: static
else
all: shared static
endif

ifdef TEST_TIMING_ALT_IMPL
OBJS_CRYPTO += external_timing_for_test.o
external_timing_for_test.o: ../tests/src/external_timing/external_timing_for_test.c
	echo "  CC    $<"
	$(CC) $(LOCAL_CFLAGS) $(CFLAGS) -o $@ -c $<
endif

static: libmbedcrypto.a libmbedx509.a libmbedtls.a
	cd ../tests && echo "This is a seedfile that contains 64 bytes (65 on Windows)......" > seedfile

shared: libmbedcrypto.$(DLEXT) libmbedx509.$(DLEXT) libmbedtls.$(DLEXT)

# Windows builds under Mingw can fail if make tries to create archives in the same
# directory at the same time - see https://bugs.launchpad.net/gcc-arm-embedded/+bug/1848002.
# This forces builds of the .a files to be serialised.
ifdef WINDOWS
libmbedtls.a: | libmbedx509.a
libmbedx509.a: | libmbedcrypto.a
endif

# tls
libmbedtls.a: $(OBJS_TLS)
	echo "  AR    $@"
	$(AR) $(ARFLAGS) $@ $(OBJS_TLS)
ifdef APPLE_BUILD
ifneq ($(APPLE_BUILD),0)
	echo "  RL    $@"
	$(RL) $(RLFLAGS) $@
endif
endif

libmbedtls.$(SOEXT_TLS): $(OBJS_TLS) libmbedx509.so
	echo "  LD    $@"
	$(CC) -shared -Wl,-soname,$@ -o $@ $(OBJS_TLS) -L. -lmbedx509 -lmbedcrypto $(LOCAL_LDFLAGS) $(LDFLAGS)

libmbedtls.so: libmbedtls.$(SOEXT_TLS)
	echo "  LN    $@ -> $<"
	ln -sf $< $@

libmbedtls.dylib: $(OBJS_TLS) libmbedx509.dylib
	echo "  LD    $@"
	$(CC) -dynamiclib -o $@ $(OBJS_TLS) -L. -lmbedx509 -lmbedcrypto $(LOCAL_LDFLAGS) $(LDFLAGS)

libmbedtls.dll: $(OBJS_TLS) libmbedx509.dll
	echo "  LD    $@"
	$(CC) -shared -Wl,-soname,$@ -Wl,--out-implib,$@.a -o $@ $(OBJS_TLS) -lws2_32 -lwinmm -lgdi32 -L. -lmbedx509 -lmbedcrypto -static-libgcc $(LOCAL_LDFLAGS) $(LDFLAGS)

# x509
libmbedx509.a: $(OBJS_X509)
	echo "  AR    $@"
	$(AR) $(ARFLAGS) $@ $(OBJS_X509)
ifdef APPLE_BUILD
ifneq ($(APPLE_BUILD),0)
	echo "  RL    $@"
	$(RL) $(RLFLAGS) $@
endif
endif

libmbedx509.$(SOEXT_X509): $(OBJS_X509) libmbedcrypto.so
	echo "  LD    $@"
	$(CC) -shared -Wl,-soname,$@ -o $@ $(OBJS_X509) -L. -lmbedcrypto $(LOCAL_LDFLAGS) $(LDFLAGS)

libmbedx509.so: libmbedx509.$(SOEXT_X509)
	echo "  LN    $@ -> $<"
	ln -sf $< $@

libmbedx509.dylib: $(OBJS_X509) libmbedcrypto.dylib
	echo "  LD    $@"
	$(CC) -dynamiclib -o $@ $(OBJS_X509) -L. -lmbedcrypto  $(LOCAL_LDFLAGS) $(LDFLAGS)

libmbedx509.dll: $(OBJS_X509) libmbedcrypto.dll
	echo "  LD    $@"
	$(CC) -shared -Wl,-soname,$@ -Wl,--out-implib,$@.a -o $@ $(OBJS_X509) -lws2_32 -lwinmm -lgdi32 -L. -lmbedcrypto -static-libgcc $(LOCAL_LDFLAGS) $(LDFLAGS)

# crypto
libmbedcrypto.a: $(OBJS_CRYPTO)
	echo "  AR    $@"
	$(AR) $(ARFLAGS) $@ $(OBJS_CRYPTO)
ifdef APPLE_BUILD
ifneq ($(APPLE_BUILD),0)
	echo "  RL    $@"
	$(RL) $(RLFLAGS) $@
endif
endif

libmbedcrypto.$(SOEXT_CRYPTO): $(OBJS_CRYPTO)
	echo "  LD    $@"
	$(CC) -shared -Wl,-soname,$@ -o $@ $(OBJS_CRYPTO) $(LOCAL_LDFLAGS) $(LDFLAGS)

libmbedcrypto.so: libmbedcrypto.$(SOEXT_CRYPTO)
	echo "  LN    $@ -> $<"
	ln -sf $< $@

libmbedcrypto.dylib: $(OBJS_CRYPTO)
	echo "  LD    $@"
	$(CC) -dynamiclib -o $@ $(OBJS_CRYPTO) $(LOCAL_LDFLAGS) $(LDFLAGS)

libmbedcrypto.dll: $(OBJS_CRYPTO)
	echo "  LD    $@"
	$(CC) -shared -Wl,-soname,$@ -Wl,--out-implib,$@.a -o $@ $(OBJS_CRYPTO) -lws2_32 -lwinmm -lgdi32 -static-libgcc $(LOCAL_LDFLAGS) $(LDFLAGS)

.c.o:
	echo "  CC    $<"
	$(CC) $(LOCAL_CFLAGS) $(CFLAGS) -o $@ -c $<

clean:
ifndef WINDOWS
	rm -f *.o libmbed*
	rm -f $(THIRDPARTY_CRYPTO_OBJECTS)
else
	if exist *.o del /Q /F *.o
	if exist libmbed* del /Q /F libmbed*
	del /Q /F del_errors_out_if_the_file_list_is_empty_but_not_if_a_file_does_not_exist $(subst /,\,$(THIRDPARTY_CRYPTO_OBJECTS))
endif
