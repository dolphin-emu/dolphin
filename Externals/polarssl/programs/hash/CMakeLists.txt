add_executable(hello hello.c)
target_link_libraries(hello polarssl)

add_executable(generic_sum generic_sum.c)
target_link_libraries(generic_sum polarssl)

add_executable(md5sum md5sum.c)
target_link_libraries(md5sum polarssl)

add_executable(sha1sum sha1sum.c)
target_link_libraries(sha1sum polarssl)

add_executable(sha2sum sha2sum.c)
target_link_libraries(sha2sum polarssl)

install(TARGETS hello md5sum sha1sum sha2sum generic_sum
        DESTINATION "bin"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
