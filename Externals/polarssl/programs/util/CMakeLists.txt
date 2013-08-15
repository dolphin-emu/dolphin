set(libs
     polarssl
)

add_executable(strerror strerror.c)
target_link_libraries(strerror ${libs})

install(TARGETS strerror
        DESTINATION "bin"
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
