option(INSTALL_POLARSSL_HEADERS "Install PolarSSL headers." ON)

if(INSTALL_POLARSSL_HEADERS)

file(GLOB headers "polarssl/*.h")

install(FILES ${headers}
        DESTINATION include/polarssl
        PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

endif(INSTALL_POLARSSL_HEADERS)
