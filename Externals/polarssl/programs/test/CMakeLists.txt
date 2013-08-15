FIND_PACKAGE(OpenSSL)

set(libs
     polarssl
)

if(USE_PKCS11_HELPER_LIBRARY)
    set(libs ${libs} pkcs11-helper)
endif(USE_PKCS11_HELPER_LIBRARY)

if(ENABLE_ZLIB_SUPPORT)
    set(libs ${libs} ${ZLIB_LIBRARIES})
endif(ENABLE_ZLIB_SUPPORT)

add_executable(selftest selftest.c)
target_link_libraries(selftest ${libs})

add_executable(benchmark benchmark.c)
target_link_libraries(benchmark ${libs})

add_executable(ssl_test ssl_test.c)
target_link_libraries(ssl_test ${libs})

add_executable(ssl_cert_test ssl_cert_test.c)
target_link_libraries(ssl_cert_test ${libs})

install(TARGETS selftest benchmark ssl_test ssl_cert_test
        DESTINATION "bin"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

if(OPENSSL_FOUND)
    add_executable(o_p_test o_p_test.c)
    target_link_libraries(o_p_test ${libs} ${OPENSSL_LIBRARIES})

    install(TARGETS o_p_test
        DESTINATION "bin"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif(OPENSSL_FOUND)

