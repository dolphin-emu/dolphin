list (APPEND thirdparty_src)
list (APPEND thirdparty_lib)
list (APPEND thirdparty_inc_public)
list (APPEND thirdparty_inc)
list (APPEND thirdparty_def)

execute_process(COMMAND ${MBEDTLS_PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/../scripts/config.py -f ${CMAKE_CURRENT_SOURCE_DIR}/../include/mbedtls/config.h get MBEDTLS_ECDH_VARIANT_EVEREST_ENABLED RESULT_VARIABLE result)

if(${result} EQUAL 0)
    add_subdirectory(everest)
endif()

set(thirdparty_src ${thirdparty_src} PARENT_SCOPE)
set(thirdparty_lib ${thirdparty_lib} PARENT_SCOPE)
set(thirdparty_inc_public ${thirdparty_inc_public} PARENT_SCOPE)
set(thirdparty_inc ${thirdparty_inc} PARENT_SCOPE)
set(thirdparty_def ${thirdparty_def} PARENT_SCOPE)
