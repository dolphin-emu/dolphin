include(ExternalProject)

if(WIN32 AND DOLPHIN_HOST STREQUAL "x64")
    # From https://mlocati.github.io/articles/gettext-iconv-windows.html, endorsed by GNU: https://www.gnu.org/software/gettext/
    dolphin_fetchcontent_declare(Gettext
        URL https://github.com/mlocati/gettext-iconv-windows/releases/download/v1.0-v1.19/gettext1.0-iconv1.19-static-64.zip
        URL_HASH SHA256=2b999bc37b56fa052ff98578e9cb188152009f634e3a66aa8ff263a016e9c466
    )
    FetchContent_MakeAvailable(Gettext)
    set(INSTALL_DIR "${gettext_SOURCE_DIR}")
else()
    if(CMAKE_GENERATOR MATCHES "Makefiles")
        set(MAKE_COMMAND "$(MAKE)")
    else()
        find_program(MAKE_PROGRAM NAMES gmake make REQUIRED)
        
        cmake_host_system_information(RESULT mem_mib QUERY TOTAL_PHYSICAL_MEMORY)
        if(NOT mem_mib OR mem_mib STREQUAL "" OR mem_mib LESS 4096)
            set(mem_mib 4096)
        endif()
        math(EXPR mem_jobs "${mem_mib} / 2048 / 2")
        
        cmake_host_system_information(RESULT cpu_count QUERY NUMBER_OF_LOGICAL_CORES)
        if(NOT cpu_count OR cpu_count STREQUAL "" OR cpu_count LESS 2)
            set(cpu_count 2)
        endif()
        math(EXPR cpu_jobs "${cpu_count} / 2")
        
        if(cpu_jobs LESS mem_jobs)
            set(jobs ${cpu_jobs})
        else()
            set(jobs ${mem_jobs})
        endif()
        set(MAKE_COMMAND ${MAKE_PROGRAM} -j${jobs})
    endif()

    ExternalProject_Add(Gettext
        URL https://ftp.gnu.org/pub/gnu/gettext/gettext-1.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE
        USES_TERMINAL_INSTALL TRUE
        CONFIGURE_COMMAND <SOURCE_DIR>/configure
            --prefix=<INSTALL_DIR>
            --disable-shared --enable-static
            --disable-java --disable-csharp --disable-d
            --disable-openmp --disable-curses --disable-modula2
            --without-emacs --without-git --without-cvs
            --enable-silent-rules
        BUILD_COMMAND ${MAKE_COMMAND}
        INSTALL_COMMAND ${MAKE_COMMAND} install
    )
    ExternalProject_Get_Property(Gettext INSTALL_DIR)
endif()

set(GETTEXT_MSGMERGE_EXECUTABLE "${INSTALL_DIR}/bin/msgmerge${CMAKE_EXECUTABLE_SUFFIX}" CACHE PATH "" FORCE)
set(GETTEXT_MSGFMT_EXECUTABLE "${INSTALL_DIR}/bin/msgfmt${CMAKE_EXECUTABLE_SUFFIX}" CACHE PATH "" FORCE)
