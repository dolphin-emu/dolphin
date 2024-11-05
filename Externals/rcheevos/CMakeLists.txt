add_library(rcheevos
	rcheevos/include/rc_api_editor.h
	rcheevos/include/rc_api_info.h
	rcheevos/include/rc_api_request.h
	rcheevos/include/rc_api_runtime.h
	rcheevos/include/rc_api_user.h
	rcheevos/include/rc_client.h
	rcheevos/include/rc_client_raintegration.h
	rcheevos/include/rc_consoles.h
	rcheevos/include/rc_error.h
	rcheevos/include/rc_export.h
	rcheevos/include/rc_hash.h
	rcheevos/include/rcheevos.h
	rcheevos/include/rc_runtime.h
	rcheevos/include/rc_runtime_types.h
	rcheevos/include/rc_url.h
	rcheevos/include/rc_util.h
	rcheevos/src/rapi/rc_api_common.c
	rcheevos/src/rapi/rc_api_common.h
	rcheevos/src/rapi/rc_api_editor.c
	rcheevos/src/rapi/rc_api_info.c
	rcheevos/src/rapi/rc_api_runtime.c
	rcheevos/src/rapi/rc_api_user.c
	rcheevos/src/rcheevos/alloc.c
	rcheevos/src/rcheevos/condition.c
	rcheevos/src/rcheevos/condset.c
	rcheevos/src/rcheevos/consoleinfo.c
	rcheevos/src/rcheevos/format.c
	rcheevos/src/rcheevos/lboard.c
	rcheevos/src/rcheevos/memref.c
	rcheevos/src/rcheevos/operand.c
	rcheevos/src/rcheevos/rc_internal.h
	rcheevos/src/rcheevos/rc_validate.c
	rcheevos/src/rcheevos/rc_validate.h
	rcheevos/src/rcheevos/richpresence.c
	rcheevos/src/rcheevos/runtime.c
	rcheevos/src/rcheevos/runtime_progress.c
	rcheevos/src/rcheevos/trigger.c
	rcheevos/src/rcheevos/value.c
	rcheevos/src/rhash/aes.c
	rcheevos/src/rhash/aes.h
	rcheevos/src/rhash/cdreader.c
	rcheevos/src/rhash/hash.c
	rcheevos/src/rhash/md5.c
	rcheevos/src/rhash/md5.h
	rcheevos/src/rurl/url.c
	rcheevos/src/rc_client.c
	rcheevos/src/rc_client_external.h
	rcheevos/src/rc_client_internal.h
	rcheevos/src/rc_client_raintegration.c
	rcheevos/src/rc_client_raintegration_internal.h
	rcheevos/src/rc_compat.c
	rcheevos/src/rc_compat.h
	rcheevos/src/rc_util.c
	rcheevos/src/rc_version.c
	rcheevos/src/rc_version.h
)
dolphin_disable_warnings(rcheevos)

target_include_directories(rcheevos PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/rcheevos/include")
target_include_directories(rcheevos INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(rcheevos PRIVATE "RC_DISABLE_LUA=1" "RCHEEVOS_URL_SSL")
target_compile_definitions(rcheevos PRIVATE "RC_CLIENT_SUPPORTS_HASH")
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
	target_compile_definitions(rcheevos PRIVATE "_CRT_SECURE_NO_WARNINGS")
endif()
