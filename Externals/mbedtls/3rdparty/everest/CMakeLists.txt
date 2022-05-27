list (APPEND everest_src)
list (APPEND everest_inc_public)
list (APPEND everest_inc)
list (APPEND everest_def)

set(everest_src
  ${CMAKE_CURRENT_SOURCE_DIR}/library/everest.c
  ${CMAKE_CURRENT_SOURCE_DIR}/library/x25519.c
  ${CMAKE_CURRENT_SOURCE_DIR}/library/Hacl_Curve25519_joined.c
)

list(APPEND everest_inc_public ${CMAKE_CURRENT_SOURCE_DIR}/include)
list(APPEND everest_inc ${CMAKE_CURRENT_SOURCE_DIR}/include/everest ${CMAKE_CURRENT_SOURCE_DIR}/include/everest/kremlib)

if(INSTALL_MBEDTLS_HEADERS)

  install(DIRECTORY include/everest
    DESTINATION include
    FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    FILES_MATCHING PATTERN "*.h")

endif(INSTALL_MBEDTLS_HEADERS)

set(thirdparty_src ${thirdparty_src} ${everest_src} PARENT_SCOPE)
set(thirdparty_inc_public ${thirdparty_inc_public} ${everest_inc_public} PARENT_SCOPE)
set(thirdparty_inc ${thirdparty_inc} ${everest_inc} PARENT_SCOPE)
set(thirdparty_def ${thirdparty_def} ${everest_def} PARENT_SCOPE)
