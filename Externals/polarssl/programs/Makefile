
# To compile on SunOS: add "-lsocket -lnsl" to LDFLAGS
# To compile on MinGW: add "-lws2_32" to LDFLAGS or define WINDOWS in your env
# To compile with PKCS11: add "-lpkcs11-helper" to LDFLAGS

CFLAGS	+= -I../include -D_FILE_OFFSET_BITS=64 -Wall -W -Wdeclaration-after-statement
OFLAGS	= -O2
LDFLAGS	+= -L../library -lpolarssl $(SYS_LDFLAGS)

ifdef DEBUG
CFLAGS += -g3
endif

ifdef WINDOWS
LDFLAGS += -lws2_32
endif

# Zlib shared library extensions:
ifdef ZLIB
LDFLAGS += -lz
endif

APPS =	aes/aescrypt2	aes/crypt_and_hash	\
	hash/hello			hash/generic_sum	\
	hash/md5sum			hash/sha1sum		\
	hash/sha2sum		pkey/dh_client		\
	pkey/dh_genprime	pkey/dh_server		\
	pkey/key_app		pkey/key_app_writer	\
	pkey/mpi_demo		pkey/rsa_genkey		\
	pkey/rsa_decrypt	pkey/rsa_encrypt	\
	pkey/rsa_sign		pkey/rsa_verify		\
	pkey/rsa_sign_pss	pkey/rsa_verify_pss \
	ssl/ssl_client1		ssl/ssl_client2		\
	ssl/ssl_server		ssl/ssl_server2		\
	ssl/ssl_fork_server						\
	ssl/ssl_mail_client	random/gen_entropy	\
	random/gen_random_havege				\
	random/gen_random_ctr_drbg				\
	test/ssl_cert_test	test/benchmark		\
	test/selftest		test/ssl_test		\
	util/strerror							\
	x509/cert_app		x509/crl_app		\
	x509/cert_req

ifdef OPENSSL
APPS +=	test/o_p_test
endif

.SILENT:

all: $(APPS)

aes/aescrypt2: aes/aescrypt2.c ../library/libpolarssl.a
	echo   "  CC    aes/aescrypt2.c"
	$(CC) $(CFLAGS) $(OFLAGS) aes/aescrypt2.c    $(LDFLAGS) -o $@

aes/crypt_and_hash: aes/crypt_and_hash.c ../library/libpolarssl.a
	echo   "  CC    aes/crypt_and_hash.c"
	$(CC) $(CFLAGS) $(OFLAGS) aes/crypt_and_hash.c $(LDFLAGS) -o $@

hash/hello: hash/hello.c ../library/libpolarssl.a
	echo   "  CC    hash/hello.c"
	$(CC) $(CFLAGS) $(OFLAGS) hash/hello.c       $(LDFLAGS) -o $@

hash/generic_sum: hash/generic_sum.c ../library/libpolarssl.a
	echo   "  CC    hash/generic_sum.c"
	$(CC) $(CFLAGS) $(OFLAGS) hash/generic_sum.c $(LDFLAGS) -o $@

hash/md5sum: hash/md5sum.c ../library/libpolarssl.a
	echo   "  CC    hash/md5sum.c"
	$(CC) $(CFLAGS) $(OFLAGS) hash/md5sum.c      $(LDFLAGS) -o $@

hash/sha1sum: hash/sha1sum.c ../library/libpolarssl.a
	echo   "  CC    hash/sha1sum.c"
	$(CC) $(CFLAGS) $(OFLAGS) hash/sha1sum.c     $(LDFLAGS) -o $@

hash/sha2sum: hash/sha2sum.c ../library/libpolarssl.a
	echo   "  CC    hash/sha2sum.c"
	$(CC) $(CFLAGS) $(OFLAGS) hash/sha2sum.c     $(LDFLAGS) -o $@

pkey/dh_client: pkey/dh_client.c ../library/libpolarssl.a
	echo   "  CC    pkey/dh_client.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/dh_client.c   $(LDFLAGS) -o $@

pkey/dh_genprime: pkey/dh_genprime.c ../library/libpolarssl.a
	echo   "  CC    pkey/dh_genprime.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/dh_genprime.c $(LDFLAGS) -o $@

pkey/dh_server: pkey/dh_server.c ../library/libpolarssl.a
	echo   "  CC    pkey/dh_server.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/dh_server.c   $(LDFLAGS) -o $@

pkey/key_app: pkey/key_app.c ../library/libpolarssl.a
	echo   "  CC    pkey/key_app.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/key_app.c   $(LDFLAGS) -o $@

pkey/key_app_writer: pkey/key_app_writer.c ../library/libpolarssl.a
	echo   "  CC    pkey/key_app_writer.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/key_app_writer.c   $(LDFLAGS) -o $@

pkey/mpi_demo: pkey/mpi_demo.c ../library/libpolarssl.a
	echo   "  CC    pkey/mpi_demo.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/mpi_demo.c    $(LDFLAGS) -o $@

pkey/rsa_genkey: pkey/rsa_genkey.c ../library/libpolarssl.a
	echo   "  CC    pkey/rsa_genkey.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/rsa_genkey.c  $(LDFLAGS) -o $@

pkey/rsa_sign: pkey/rsa_sign.c ../library/libpolarssl.a
	echo   "  CC    pkey/rsa_sign.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/rsa_sign.c    $(LDFLAGS) -o $@

pkey/rsa_verify: pkey/rsa_verify.c ../library/libpolarssl.a
	echo   "  CC    pkey/rsa_verify.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/rsa_verify.c  $(LDFLAGS) -o $@

pkey/rsa_sign_pss: pkey/rsa_sign_pss.c ../library/libpolarssl.a
	echo   "  CC    pkey/rsa_sign_pss.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/rsa_sign_pss.c    $(LDFLAGS) -o $@

pkey/rsa_verify_pss: pkey/rsa_verify_pss.c ../library/libpolarssl.a
	echo   "  CC    pkey/rsa_verify_pss.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/rsa_verify_pss.c  $(LDFLAGS) -o $@

pkey/rsa_decrypt: pkey/rsa_decrypt.c ../library/libpolarssl.a
	echo   "  CC    pkey/rsa_decrypt.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/rsa_decrypt.c    $(LDFLAGS) -o $@

pkey/rsa_encrypt: pkey/rsa_encrypt.c ../library/libpolarssl.a
	echo   "  CC    pkey/rsa_encrypt.c"
	$(CC) $(CFLAGS) $(OFLAGS) pkey/rsa_encrypt.c    $(LDFLAGS) -o $@

random/gen_entropy: random/gen_entropy.c ../library/libpolarssl.a
	echo   "  CC    random/gen_entropy.c"
	$(CC) $(CFLAGS) $(OFLAGS) random/gen_entropy.c $(LDFLAGS) -o $@

random/gen_random_havege: random/gen_random_havege.c ../library/libpolarssl.a
	echo   "  CC    random/gen_random_havege.c"
	$(CC) $(CFLAGS) $(OFLAGS) random/gen_random_havege.c $(LDFLAGS) -o $@

random/gen_random_ctr_drbg: random/gen_random_ctr_drbg.c ../library/libpolarssl.a
	echo   "  CC    random/gen_random_ctr_drbg.c"
	$(CC) $(CFLAGS) $(OFLAGS) random/gen_random_ctr_drbg.c $(LDFLAGS) -o $@

ssl/ssl_client1: ssl/ssl_client1.c ../library/libpolarssl.a
	echo   "  CC    ssl/ssl_client1.c"
	$(CC) $(CFLAGS) $(OFLAGS) ssl/ssl_client1.c  $(LDFLAGS) -o $@

ssl/ssl_client2: ssl/ssl_client2.c ../library/libpolarssl.a
	echo   "  CC    ssl/ssl_client2.c"
	$(CC) $(CFLAGS) $(OFLAGS) ssl/ssl_client2.c  $(LDFLAGS) -o $@

ssl/ssl_server: ssl/ssl_server.c ../library/libpolarssl.a
	echo   "  CC    ssl/ssl_server.c"
	$(CC) $(CFLAGS) $(OFLAGS) ssl/ssl_server.c   $(LDFLAGS) -o $@

ssl/ssl_server2: ssl/ssl_server2.c ../library/libpolarssl.a
	echo   "  CC    ssl/ssl_server2.c"
	$(CC) $(CFLAGS) $(OFLAGS) ssl/ssl_server2.c   $(LDFLAGS) -o $@

ssl/ssl_fork_server: ssl/ssl_fork_server.c ../library/libpolarssl.a
	echo   "  CC    ssl/ssl_fork_server.c"
	$(CC) $(CFLAGS) $(OFLAGS) ssl/ssl_fork_server.c   $(LDFLAGS) -o $@

ssl/ssl_mail_client: ssl/ssl_mail_client.c ../library/libpolarssl.a
	echo   "  CC    ssl/ssl_mail_client.c"
	$(CC) $(CFLAGS) $(OFLAGS) ssl/ssl_mail_client.c   $(LDFLAGS) -o $@

test/ssl_cert_test: test/ssl_cert_test.c ../library/libpolarssl.a
	echo   "  CC    test/ssl_cert_test.c"
	$(CC) $(CFLAGS) $(OFLAGS) test/ssl_cert_test.c   $(LDFLAGS) -o $@

test/benchmark: test/benchmark.c ../library/libpolarssl.a
	echo   "  CC    test/benchmark.c"
	$(CC) $(CFLAGS) $(OFLAGS) test/benchmark.c   $(LDFLAGS) -o $@

test/selftest: test/selftest.c ../library/libpolarssl.a
	echo   "  CC    test/selftest.c"
	$(CC) $(CFLAGS) $(OFLAGS) test/selftest.c    $(LDFLAGS) -o $@

test/ssl_test: test/ssl_test.c ../library/libpolarssl.a
	echo   "  CC    test/ssl_test.c"
	$(CC) $(CFLAGS) $(OFLAGS) test/ssl_test.c    $(LDFLAGS) -o $@

test/o_p_test: test/o_p_test.c ../library/libpolarssl.a
	echo   "  CC    test/o_p_test.c"
	$(CC) $(CFLAGS) $(OFLAGS) test/o_p_test.c    $(LDFLAGS) -o $@ -lssl -lcrypto

util/strerror: util/strerror.c ../library/libpolarssl.a
	echo   "  CC    util/strerror.c"
	$(CC) $(CFLAGS) $(OFLAGS) util/strerror.c    $(LDFLAGS) -o $@

x509/cert_app: x509/cert_app.c ../library/libpolarssl.a
	echo   "  CC    x509/cert_app.c"
	$(CC) $(CFLAGS) $(OFLAGS) x509/cert_app.c    $(LDFLAGS) -o $@

x509/crl_app: x509/crl_app.c ../library/libpolarssl.a
	echo   "  CC    x509/crl_app.c"
	$(CC) $(CFLAGS) $(OFLAGS) x509/crl_app.c    $(LDFLAGS) -o $@

x509/cert_req: x509/cert_req.c ../library/libpolarssl.a
	echo   "  CC    x509/cert_req.c"
	$(CC) $(CFLAGS) $(OFLAGS) x509/cert_req.c    $(LDFLAGS) -o $@

clean:
ifndef WINDOWS
	rm -f $(APPS)
endif
ifdef WINDOWS
	del /S /Q /F *.o *.exe
endif
