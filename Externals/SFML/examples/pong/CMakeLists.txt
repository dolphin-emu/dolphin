
set(SRCROOT ${PROJECT_SOURCE_DIR}/examples/pong)

# all source files
set(SRC ${SRCROOT}/Pong.cpp)

# define the pong target
sfml_add_example(pong GUI_APP
                 SOURCES ${SRC}
                 DEPENDS sfml-audio sfml-graphics sfml-window sfml-system)
