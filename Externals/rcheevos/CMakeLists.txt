dolphin_fetchcontent_declare(
  rcheevos
  GIT_REPOSITORY https://github.com/RetroAchievements/rcheevos.git
  GIT_TAG 926e4608f8dca7989267c787bbefb3ab1c835ac5
)
FetchContent_MakeAvailable(rcheevos)

add_library(rcheevos
  ${rcheevos_SOURCE_DIR}/include/rc_api_editor.h
  ${rcheevos_SOURCE_DIR}/include/rc_api_info.h
  ${rcheevos_SOURCE_DIR}/include/rc_api_request.h
  ${rcheevos_SOURCE_DIR}/include/rc_api_runtime.h
  ${rcheevos_SOURCE_DIR}/include/rc_api_user.h
  ${rcheevos_SOURCE_DIR}/include/rc_client.h
  ${rcheevos_SOURCE_DIR}/include/rc_client_raintegration.h
  ${rcheevos_SOURCE_DIR}/include/rc_consoles.h
  ${rcheevos_SOURCE_DIR}/include/rc_error.h
  ${rcheevos_SOURCE_DIR}/include/rc_export.h
  ${rcheevos_SOURCE_DIR}/include/rc_hash.h
  ${rcheevos_SOURCE_DIR}/include/rcheevos.h
  ${rcheevos_SOURCE_DIR}/include/rc_runtime.h
  ${rcheevos_SOURCE_DIR}/include/rc_runtime_types.h
  ${rcheevos_SOURCE_DIR}/include/rc_util.h
  ${rcheevos_SOURCE_DIR}/src/rapi/rc_api_common.c
  ${rcheevos_SOURCE_DIR}/src/rapi/rc_api_common.h
  ${rcheevos_SOURCE_DIR}/src/rapi/rc_api_editor.c
  ${rcheevos_SOURCE_DIR}/src/rapi/rc_api_info.c
  ${rcheevos_SOURCE_DIR}/src/rapi/rc_api_runtime.c
  ${rcheevos_SOURCE_DIR}/src/rapi/rc_api_user.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/alloc.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/condition.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/condset.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/consoleinfo.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/format.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/lboard.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/memref.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/operand.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/rc_internal.h
  ${rcheevos_SOURCE_DIR}/src/rcheevos/rc_validate.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/rc_validate.h
  ${rcheevos_SOURCE_DIR}/src/rcheevos/richpresence.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/runtime.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/runtime_progress.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/trigger.c
  ${rcheevos_SOURCE_DIR}/src/rcheevos/value.c
  ${rcheevos_SOURCE_DIR}/src/rhash/aes.c
  ${rcheevos_SOURCE_DIR}/src/rhash/aes.h
  ${rcheevos_SOURCE_DIR}/src/rhash/cdreader.c
  ${rcheevos_SOURCE_DIR}/src/rhash/hash.c
  ${rcheevos_SOURCE_DIR}/src/rhash/hash_disc.c
  ${rcheevos_SOURCE_DIR}/src/rhash/md5.c
  ${rcheevos_SOURCE_DIR}/src/rhash/md5.h
  ${rcheevos_SOURCE_DIR}/src/rhash/rc_hash_internal.h
  ${rcheevos_SOURCE_DIR}/src/rc_client.c
  ${rcheevos_SOURCE_DIR}/src/rc_client_external.c
  ${rcheevos_SOURCE_DIR}/src/rc_client_external.h
  ${rcheevos_SOURCE_DIR}/src/rc_client_external_versions.h
  ${rcheevos_SOURCE_DIR}/src/rc_client_internal.h
  ${rcheevos_SOURCE_DIR}/src/rc_client_raintegration.c
  ${rcheevos_SOURCE_DIR}/src/rc_client_raintegration_internal.h
  ${rcheevos_SOURCE_DIR}/src/rc_compat.c
  ${rcheevos_SOURCE_DIR}/src/rc_compat.h
  ${rcheevos_SOURCE_DIR}/src/rc_util.c
  ${rcheevos_SOURCE_DIR}/src/rc_version.c
  ${rcheevos_SOURCE_DIR}/src/rc_version.h
)

target_include_directories(rcheevos PUBLIC "${rcheevos_SOURCE_DIR}/include")
target_compile_definitions(rcheevos PRIVATE "RC_DISABLE_LUA=1" "RCHEEVOS_URL_SSL")
target_compile_definitions(rcheevos PRIVATE "RC_CLIENT_SUPPORTS_HASH")
target_compile_definitions(rcheevos PRIVATE "RC_CLIENT_SUPPORTS_EXTERNAL")
target_compile_definitions(rcheevos PRIVATE "RC_HASH_NO_ENCRYPTED")
target_compile_definitions(rcheevos PRIVATE "RC_HASH_NO_ROM")
target_compile_definitions(rcheevos PRIVATE "RC_HASH_NO_ZIP")
if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  target_compile_definitions(rcheevos PRIVATE "RC_CLIENT_SUPPORTS_RAINTEGRATION")
  target_compile_definitions(rcheevos PRIVATE "_CRT_SECURE_NO_WARNINGS")
endif()
