dolphin_fetchcontent_declare(
  wil
  GIT_REPOSITORY https://github.com/microsoft/wil.git
  GIT_TAG 7447d38337abf23a5c2286305b743506455aa9cc
)

set(WIL_BUILD_PACKAGING OFF)
set(WIL_BUILD_TESTS OFF)

FetchContent_MakeAvailable(wil)
