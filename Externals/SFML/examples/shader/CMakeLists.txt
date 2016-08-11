
set(SRCROOT ${PROJECT_SOURCE_DIR}/examples/shader)

# all source files
set(SRC
    ${SRCROOT}/Effect.hpp
    ${SRCROOT}/Shader.cpp)

# define the shader target
sfml_add_example(shader GUI_APP
                 SOURCES ${SRC}
                 DEPENDS sfml-graphics sfml-window sfml-system)
