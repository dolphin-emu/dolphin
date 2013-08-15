
# To compile on SunOS: add "-lsocket -lnsl" to LDFLAGS
# To compile on MinGW: add "-lws2_32" to LDFLAGS or define WINDOWS in your env
# To compile with PKCS11: add "-lpkcs11-helper" to LDFLAGS

CFLAGS	+= -I../include -D_FILE_OFFSET_BITS=64 -Wall -W -Wdeclaration-after-statement \
			-Wno-unused-function -Wno-unused-value

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

APPS =	test_suite_aes.ecb		test_suite_aes.cbc		\
		test_suite_aes.cfb		test_suite_aes.rest		\
		test_suite_arc4									\
		test_suite_base64		test_suite_blowfish		\
		test_suite_camellia		test_suite_cipher.aes	\
		test_suite_cipher.blowfish						\
		test_suite_cipher.camellia						\
		test_suite_cipher.des	test_suite_cipher.null	\
		test_suite_ctr_drbg		test_suite_debug		\
		test_suite_des			test_suite_dhm			\
		test_suite_error								\
		test_suite_gcm.decrypt_128						\
		test_suite_gcm.decrypt_192						\
		test_suite_gcm.decrypt_256						\
		test_suite_gcm.encrypt_128						\
		test_suite_gcm.encrypt_192						\
		test_suite_gcm.encrypt_256						\
		test_suite_hmac_shax							\
		test_suite_md			test_suite_mdx			\
		test_suite_mpi			test_suite_pbkdf2		\
		test_suite_pkcs1_v21	test_suite_pkcs5		\
		test_suite_rsa			test_suite_shax			\
		test_suite_x509parse	test_suite_x509write	\
		test_suite_xtea			test_suite_version

.SILENT:

all: $(APPS)

test_suite_aes.ecb.c : suites/test_suite_aes.function suites/test_suite_aes.ecb.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_aes test_suite_aes.ecb

test_suite_aes.cbc.c : suites/test_suite_aes.function suites/test_suite_aes.cbc.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_aes test_suite_aes.cbc

test_suite_aes.cfb.c : suites/test_suite_aes.function suites/test_suite_aes.cfb.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_aes test_suite_aes.cfb

test_suite_aes.rest.c : suites/test_suite_aes.function suites/test_suite_aes.rest.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_aes test_suite_aes.rest

test_suite_cipher.aes.c : suites/test_suite_cipher.function suites/test_suite_cipher.aes.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_cipher test_suite_cipher.aes

test_suite_cipher.blowfish.c : suites/test_suite_cipher.function suites/test_suite_cipher.blowfish.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_cipher test_suite_cipher.blowfish

test_suite_cipher.camellia.c : suites/test_suite_cipher.function suites/test_suite_cipher.camellia.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_cipher test_suite_cipher.camellia

test_suite_cipher.des.c : suites/test_suite_cipher.function suites/test_suite_cipher.des.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_cipher test_suite_cipher.des

test_suite_cipher.null.c : suites/test_suite_cipher.function suites/test_suite_cipher.null.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_cipher test_suite_cipher.null

test_suite_gcm.decrypt_128.c : suites/test_suite_gcm.function suites/test_suite_gcm.decrypt_128.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_gcm test_suite_gcm.decrypt_128

test_suite_gcm.decrypt_192.c : suites/test_suite_gcm.function suites/test_suite_gcm.decrypt_192.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_gcm test_suite_gcm.decrypt_192

test_suite_gcm.decrypt_256.c : suites/test_suite_gcm.function suites/test_suite_gcm.decrypt_256.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_gcm test_suite_gcm.decrypt_256

test_suite_gcm.encrypt_128.c : suites/test_suite_gcm.function suites/test_suite_gcm.encrypt_128.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_gcm test_suite_gcm.encrypt_128

test_suite_gcm.encrypt_192.c : suites/test_suite_gcm.function suites/test_suite_gcm.encrypt_192.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_gcm test_suite_gcm.encrypt_192

test_suite_gcm.encrypt_256.c : suites/test_suite_gcm.function suites/test_suite_gcm.encrypt_256.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites test_suite_gcm test_suite_gcm.encrypt_256

%.c : suites/%.function suites/%.data scripts/generate_code.pl suites/helpers.function
	echo   "  Generate	$@"
	scripts/generate_code.pl suites $* $*

test_suite_aes.ecb: test_suite_aes.ecb.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_aes.cbc: test_suite_aes.cbc.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_aes.cfb: test_suite_aes.cfb.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_aes.rest: test_suite_aes.rest.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_arc4: test_suite_arc4.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_base64: test_suite_base64.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_blowfish: test_suite_blowfish.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_camellia: test_suite_camellia.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_cipher.aes: test_suite_cipher.aes.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_cipher.blowfish: test_suite_cipher.blowfish.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_cipher.camellia: test_suite_cipher.camellia.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_cipher.des: test_suite_cipher.des.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_cipher.null: test_suite_cipher.null.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_ctr_drbg: test_suite_ctr_drbg.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_des: test_suite_des.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_dhm: test_suite_dhm.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_error: test_suite_error.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_gcm.decrypt_128: test_suite_gcm.decrypt_128.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_gcm.decrypt_192: test_suite_gcm.decrypt_192.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_gcm.decrypt_256: test_suite_gcm.decrypt_256.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_gcm.encrypt_128: test_suite_gcm.encrypt_128.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_gcm.encrypt_192: test_suite_gcm.encrypt_192.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_gcm.encrypt_256: test_suite_gcm.encrypt_256.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_hmac_shax: test_suite_hmac_shax.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_md: test_suite_md.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_mdx: test_suite_mdx.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_mpi: test_suite_mpi.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_pbkdf2: test_suite_pbkdf2.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_pkcs1_v21: test_suite_pkcs1_v21.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_pkcs5: test_suite_pkcs5.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_rsa: test_suite_rsa.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_shax: test_suite_shax.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_x509parse: test_suite_x509parse.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_x509write: test_suite_x509write.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_xtea: test_suite_xtea.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_debug: test_suite_debug.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

test_suite_version: test_suite_version.c ../library/libpolarssl.a
	echo   "  CC    	$@.c"
	$(CC) $(CFLAGS) $(OFLAGS) $@.c	$(LDFLAGS) -o $@

clean:
ifndef WINDOWS
	rm -f $(APPS) *.c
endif
ifdef WINDOWS
	del /Q /F *.c *.exe
endif

check: $(APPS)
ifndef WINDOWS
	echo "Running checks (Success if all tests PASSED)"	
	RETURN=0;																\
	for i in $(APPS);														\
	do																		\
		echo " - $${i}";													\
		RESULT=`./$${i} | grep -v 'PASS$$' | grep -v -- '-----' | grep -v '^$$'`;	\
		FAILED=`echo $$RESULT |grep FAILED`; 								\
		echo "   $$RESULT";													\
		if [ "$$FAILED" != "" ];											\
		then																\
			echo "**** Failed ***************";								\
			RETURN=1;														\
		fi; 																\
		echo "";															\
	done;																	\
	if [ "$$RETURN" -eq 1 ]; then exit 1; fi
endif
