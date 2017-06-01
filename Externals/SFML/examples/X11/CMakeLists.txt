
set(SRCROOT ${PROJECT_SOURCE_DIR}/examples/X11)

# all source files
set(SRC ${SRCROOT}/X11.cpp)

# find OpenGL and X11
find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})
find_package(X11 REQUIRED)
include_directories(${X11_INCLUDE_DIR})

# define the X11 target
sfml_add_example(X11 GUI_APP
                 SOURCES ${SRC}
                 DEPENDS sfml-window sfml-system ${OPENGL_LIBRARIES} ${X11_LIBRARIES})
