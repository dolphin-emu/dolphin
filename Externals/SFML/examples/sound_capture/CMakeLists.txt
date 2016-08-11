
set(SRCROOT ${PROJECT_SOURCE_DIR}/examples/sound_capture)

# all source files
set(SRC ${SRCROOT}/SoundCapture.cpp)

# define the sound-capture target
sfml_add_example(sound-capture
                 SOURCES ${SRC}
                 DEPENDS sfml-audio sfml-system)
