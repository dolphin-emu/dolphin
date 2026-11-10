dolphin_fetchcontent_declare(
  SFML
  GIT_REPOSITORY https://github.com/SFML/SFML.git
  GIT_TAG 0fa201c969e48ecc253581c5841ce73f44d42f49 # 3.0.2
  GIT_SHALLOW TRUE
)

set(BUILD_SHARED_LIBS OFF)
set(SFML_BUILD_WINDOW OFF)
set(SFML_BUILD_GRAPHICS OFF)
set(SFML_BUILD_AUDIO OFF)
set(SFML_BUILD_NETWORK ON)

FetchContent_MakeAvailable(SFML)

if(NOT MSVC)
  target_compile_options(sfml-system PRIVATE -fexceptions)
  target_compile_options(sfml-network PRIVATE -fexceptions)
endif()
