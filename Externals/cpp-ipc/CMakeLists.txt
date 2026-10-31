dolphin_fetchcontent_declare(
  cpp-ipc 
  GIT_REPOSITORY https://github.com/mutouyun/cpp-ipc.git
  GIT_TAG b6cbbc940e25ff898f09a40b107781cef6027b13
)
FetchContent_MakeAvailable(cpp-ipc)

if (NOT MSVC)
  target_compile_options(ipc PRIVATE "-fexceptions")
endif ()

add_library(cpp-ipc::ipc ALIAS ipc)
