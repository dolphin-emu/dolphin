
set(INCROOT ${PROJECT_SOURCE_DIR}/include/SFML/Audio)
set(SRCROOT ${PROJECT_SOURCE_DIR}/src/SFML/Audio)

# all source files
set(SRC
    ${SRCROOT}/ALCheck.cpp
    ${SRCROOT}/ALCheck.hpp
    ${SRCROOT}/AlResource.cpp
    ${INCROOT}/AlResource.hpp
    ${SRCROOT}/AudioDevice.cpp
    ${SRCROOT}/AudioDevice.hpp
    ${INCROOT}/Export.hpp
    ${SRCROOT}/Listener.cpp
    ${INCROOT}/Listener.hpp
    ${SRCROOT}/Music.cpp
    ${INCROOT}/Music.hpp
    ${SRCROOT}/Sound.cpp
    ${INCROOT}/Sound.hpp
    ${SRCROOT}/SoundBuffer.cpp
    ${INCROOT}/SoundBuffer.hpp
    ${SRCROOT}/SoundBufferRecorder.cpp
    ${INCROOT}/SoundBufferRecorder.hpp
    ${SRCROOT}/InputSoundFile.cpp
    ${INCROOT}/InputSoundFile.hpp
    ${SRCROOT}/OutputSoundFile.cpp
    ${INCROOT}/OutputSoundFile.hpp
    ${SRCROOT}/SoundRecorder.cpp
    ${INCROOT}/SoundRecorder.hpp
    ${SRCROOT}/SoundSource.cpp
    ${INCROOT}/SoundSource.hpp
    ${SRCROOT}/SoundStream.cpp
    ${INCROOT}/SoundStream.hpp
)
source_group("" FILES ${SRC})

set(CODECS_SRC
    ${SRCROOT}/SoundFileFactory.cpp
    ${INCROOT}/SoundFileFactory.hpp
    ${INCROOT}/SoundFileFactory.inl
    ${INCROOT}/SoundFileReader.hpp
    ${SRCROOT}/SoundFileReaderFlac.hpp
    ${SRCROOT}/SoundFileReaderFlac.cpp
    ${SRCROOT}/SoundFileReaderOgg.hpp
    ${SRCROOT}/SoundFileReaderOgg.cpp
    ${SRCROOT}/SoundFileReaderWav.hpp
    ${SRCROOT}/SoundFileReaderWav.cpp
    ${INCROOT}/SoundFileWriter.hpp
    ${SRCROOT}/SoundFileWriterFlac.hpp
    ${SRCROOT}/SoundFileWriterFlac.cpp
    ${SRCROOT}/SoundFileWriterOgg.hpp
    ${SRCROOT}/SoundFileWriterOgg.cpp
    ${SRCROOT}/SoundFileWriterWav.hpp
    ${SRCROOT}/SoundFileWriterWav.cpp
)
source_group("codecs" FILES ${CODECS_SRC})

# let CMake know about our additional audio libraries paths (on Windows and OSX)
if(SFML_OS_WINDOWS)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${PROJECT_SOURCE_DIR}/extlibs/headers/AL")
elseif(SFML_OS_MACOSX)
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-osx/Frameworks")
elseif(SFML_OS_ANDROID)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${PROJECT_SOURCE_DIR}/extlibs/headers/AL")
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${PROJECT_SOURCE_DIR}/extlibs/android")
endif()

# find external libraries
if(NOT SFML_OS_ANDROID)
    if(NOT SFML_OS_IOS)
        find_package(OpenAL REQUIRED)
    endif()
    find_package(Vorbis REQUIRED)
    find_package(FLAC REQUIRED)
else()
    find_host_package(OpenAL REQUIRED)
    find_host_package(Vorbis REQUIRED)
    find_host_package(FLAC REQUIRED)
endif()

if(NOT SFML_OS_IOS)
    include_directories(${OPENAL_INCLUDE_DIR})
endif()
include_directories(${VORBIS_INCLUDE_DIRS})
include_directories(${FLAC_INCLUDE_DIR})
add_definitions(-DOV_EXCLUDE_STATIC_CALLBACKS) # avoids warnings in vorbisfile.h
add_definitions(-DFLAC__NO_DLL)

# build the list of external libraries to link
if(SFML_OS_IOS)
    list(APPEND AUDIO_EXT_LIBS "-framework OpenAL")
else()
    list(APPEND AUDIO_EXT_LIBS ${OPENAL_LIBRARY})
endif()
if(SFML_OS_ANDROID)
    list(APPEND AUDIO_EXT_LIBS android OpenSLES)
endif()
list(APPEND AUDIO_EXT_LIBS ${VORBIS_LIBRARIES} ${FLAC_LIBRARY})

# define the sfml-audio target
sfml_add_library(sfml-audio
                 SOURCES ${SRC} ${CODECS_SRC}
                 DEPENDS sfml-system
                 EXTERNAL_LIBS ${AUDIO_EXT_LIBS})
