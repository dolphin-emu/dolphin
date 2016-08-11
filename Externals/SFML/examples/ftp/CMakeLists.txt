
set(SRCROOT ${PROJECT_SOURCE_DIR}/examples/ftp)

# all source files
set(SRC ${SRCROOT}/Ftp.cpp)

# define the ftp target
sfml_add_example(ftp
                 SOURCES ${SRC}
                 DEPENDS sfml-network sfml-system)
