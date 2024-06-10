find_program(CCACHE_BIN NAMES ccache sccache)
if(CCACHE_BIN)
    # Official ccache recommendation is to set CMAKE_C(XX)_COMPILER_LAUNCHER
    if (NOT CMAKE_C_COMPILER_LAUNCHER MATCHES "ccache")
        list(INSERT CMAKE_C_COMPILER_LAUNCHER 0 "${CCACHE_BIN}")
    endif()

    if (NOT CMAKE_CXX_COMPILER_LAUNCHER MATCHES "ccache")
        list(INSERT CMAKE_CXX_COMPILER_LAUNCHER 0 "${CCACHE_BIN}")
    endif()

    # ccache uses -I when compiling without preprocessor, which makes clang complain.
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Qunused-arguments -fcolor-diagnostics")
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments -fcolor-diagnostics")
    endif()
endif()
