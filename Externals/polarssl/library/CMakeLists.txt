set(src
     aes.c
     aesni.c
     arc4.c
     asn1parse.c
     asn1write.c
     base64.c
     bignum.c
     blowfish.c
     camellia.c
     certs.c
     cipher.c
     cipher_wrap.c
     ctr_drbg.c
     debug.c
     des.c
     dhm.c
     ecp.c
     ecp_curves.c
     ecdh.c
     ecdsa.c
     entropy.c
     entropy_poll.c
     error.c
     gcm.c
     havege.c
     md.c
     md_wrap.c
     md2.c
     md4.c
     md5.c
     memory.c
     memory_buffer_alloc.c
     net.c
     oid.c
     padlock.c
     pbkdf2.c
     pem.c
     pkcs5.c
     pkcs11.c
     pkcs12.c
     pk.c
     pk_wrap.c
     pkparse.c
     pkwrite.c
     ripemd160.c
     rsa.c
     sha1.c
     sha256.c
     sha512.c
     ssl_cache.c
     ssl_ciphersuites.c
     ssl_cli.c
     ssl_srv.c
     ssl_tls.c
     threading.c
     timing.c
     version.c
     x509.c
     x509_crt.c
     x509_crl.c
     x509_csr.c
     x509_create.c
     x509write_crt.c
     x509write_csr.c
     xtea.c
)

if(WIN32)
set(libs ws2_32)
endif(WIN32)

if(CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_C_FLAGS_CHECK "${CMAKE_C_FLAGS_CHECK} -Wmissing-declarations -Wmissing-prototypes")
  set(CMAKE_C_FLAGS_CHECKFULL "${CMAKE_C_FLAGS_CHECK} -Wcast-qual")
endif(CMAKE_COMPILER_IS_GNUCC)

add_library(polarssl STATIC ${src})

target_link_libraries(polarssl ${libs})

if(ZLIB_FOUND)
target_link_libraries(polarssl ${ZLIB_LIBRARIES})
endif(ZLIB_FOUND)

install(TARGETS polarssl
        DESTINATION ${LIB_INSTALL_DIR}
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
