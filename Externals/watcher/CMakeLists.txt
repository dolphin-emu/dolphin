dolphin_fetchcontent_declare(
  watcher
  GIT_REPOSITORY https://github.com/e-dant/watcher.git
  GIT_TAG b03bdcfc11549df595b77239cefe2643943a3e2f
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR nonexistent   # Skip upstream's broken CMakeLists.txt
)
FetchContent_MakeAvailable(watcher)

add_library(watcher INTERFACE IMPORTED GLOBAL)
set_target_properties(watcher PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${watcher_SOURCE_DIR}/include"
)
