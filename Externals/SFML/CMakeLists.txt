cmake_minimum_required(VERSION 2.8.3)

# define a macro that helps defining an option
macro(sfml_set_option var default type docstring)
    if(NOT DEFINED ${var})
        set(${var} ${default})
    endif()
    set(${var} ${${var}} CACHE ${type} ${docstring} FORCE)
endmacro()

# these options have to be set before CMake detects/configures the toolchain

# determine whether to create a debug or release build
sfml_set_option(CMAKE_BUILD_TYPE Release STRING "Choose the type of build (Debug or Release)")

# set Android specific options

# define the minimum API level to be used
sfml_set_option(ANDROID_API_MIN 9 STRING "Choose the Android API level to be used (minimum 9)")
# mirror the setting for the toolchain file
set(ANDROID_NATIVE_API_LEVEL ${ANDROID_API_MIN})

# define the path to the Android NDK
sfml_set_option(ANDROID_NDK "$ENV{ANDROID_NDK}" PATH "Path to the Android NDK")

# define the STL implementation to be used
sfml_set_option(ANDROID_STL c++_shared STRING "Choose the STL implementation to be used (experimental)")

# default the ABI to ARM v7a for hardware floating point
if(NOT ANDROID_ABI)
    set(ANDROID_ABI armeabi-v7a)
endif()

# project name
project(SFML)

# include the configuration file
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake)

# setup version numbers
set(VERSION_MAJOR 2)
set(VERSION_MINOR 4)
set(VERSION_PATCH 0)

# add the SFML header path
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# add an option for choosing the build type (shared or static)
if(NOT (SFML_OS_IOS OR SFML_OS_ANDROID))
    sfml_set_option(BUILD_SHARED_LIBS TRUE BOOL "TRUE to build SFML as shared libraries, FALSE to build it as static libraries")
else()
    if(SFML_OS_IOS)
        set(BUILD_SHARED_LIBS FALSE)
    elseif(SFML_OS_ANDROID)
        set(BUILD_SHARED_LIBS TRUE)
    endif()
endif()

# add an option for building the examples
if(NOT (SFML_OS_IOS OR SFML_OS_ANDROID))
    sfml_set_option(SFML_BUILD_EXAMPLES FALSE BOOL "TRUE to build the SFML examples, FALSE to ignore them")
else()
    set(SFML_BUILD_EXAMPLES FALSE)
endif()

# add an option for building the API documentation
sfml_set_option(SFML_BUILD_DOC FALSE BOOL "TRUE to generate the API documentation, FALSE to ignore it")

# add an option for choosing the OpenGL implementation
sfml_set_option(SFML_OPENGL_ES ${OPENGL_ES} BOOL "TRUE to use an OpenGL ES implementation, FALSE to use a desktop OpenGL implementation")

# Mac OS X specific options
if(SFML_OS_MACOSX)
    # add an option to build frameworks instead of dylibs (release only)
    sfml_set_option(SFML_BUILD_FRAMEWORKS FALSE BOOL "TRUE to build SFML as frameworks libraries (release only), FALSE to build according to BUILD_SHARED_LIBS")

    # add an option to let the user specify a custom directory for frameworks installation (SFML, FLAC, ...)
    sfml_set_option(CMAKE_INSTALL_FRAMEWORK_PREFIX "/Library/Frameworks" STRING "Frameworks installation directory")

    # add an option to automatically install Xcode templates
    sfml_set_option(SFML_INSTALL_XCODE_TEMPLATES FALSE BOOL "TRUE to automatically install the Xcode templates, FALSE to do nothing about it. The templates are compatible with Xcode 4 and 5.")
endif()

# Android options
if(SFML_OS_ANDROID)
    # make sure there's the android library available
    if (${ANDROID_API_MIN} LESS 9)
        message(FATAL_ERROR "Android API level must be equal or greater than 9. Please adjust the CMake variable 'ANDROID_API_MIN'.")
    endif()

    if(NOT ANDROID_NDK)
        message(FATAL_ERROR "The Android NDK couldn't be found. Please adjust the CMake variable 'ANDROID_NDK' to point to the NDK directory.")
    endif()

    # CMake doesn't support defining the STL to be used with Nsight Tegra, so warn the user
    if(CMAKE_VS_PLATFORM_NAME STREQUAL "Tegra-Android")
        message(WARNING "CMake might not properly support setting the STL. Make sure to adjust all generated library projects!")
    endif()

    # install everything in $NDK/sources/ because this path is appended by the NDK (convenient)
    set(CMAKE_INSTALL_PREFIX ${ANDROID_NDK}/sources/sfml)

    # we install libs in a subdirectory named after the ABI (lib/mips/*.so)
    set(LIB_SUFFIX "/${ANDROID_ABI}")

    # pass shared STL configuration (if any)
    if (ANDROID_STL MATCHES "_shared")
        add_definitions("-DSTL_LIBRARY=${ANDROID_STL}")
    endif()

    # this is a workaround to compile sfml-activity without the stl library as a dependency
    # we save the original compilation command line to restore it later in Macro.cmake
    set(CMAKE_CXX_CREATE_SHARED_LIBRARY_WITH_STL ${CMAKE_CXX_CREATE_SHARED_LIBRARY})
    set(CMAKE_CXX_CREATE_SHARED_LIBRARY_WITHOUT_STL "<CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> <CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS> <SONAME_FLAG><TARGET_SONAME> -o <TARGET> <OBJECTS> <LINK_LIBRARIES>")
else()
    unset(ANDROID_ABI CACHE)
    unset(ANDROID_API_MIN CACHE)
    unset(ANDROID_STL CACHE)
    unset(ANDROID_NATIVE_API_LEVEL CACHE)
    unset(ANDROID_NDK CACHE)
endif()

# define SFML_STATIC if the build type is not set to 'shared'
if(NOT BUILD_SHARED_LIBS)
    add_definitions(-DSFML_STATIC)
endif()

# Visual C++: remove warnings regarding SL security and algorithms on pointers
if(SFML_COMPILER_MSVC)
    # add an option to choose whether PDB debug symbols should be generated (defaults to true when possible)
    if(CMAKE_VERSION VERSION_LESS 3.1)
        sfml_set_option(SFML_GENERATE_PDB FALSE BOOL "True to generate PDB debug symbols, FALSE otherwise. Requires CMake 3.1.")
        if(SFML_GENERATE_PDB)
            message(FATAL_ERROR "Generation of PDB files (SFML_GENERATE_PDB) requires at least CMake 3.1.0")
        endif()
    else()
        sfml_set_option(SFML_GENERATE_PDB TRUE BOOL "True to generate PDB debug symbols, FALSE otherwise. Requires CMake 3.1.")
    endif()

    add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_SCL_SECURE_NO_WARNINGS)
endif()

# define SFML_OPENGL_ES if needed
if(SFML_OPENGL_ES)
    add_definitions(-DSFML_OPENGL_ES)
    add_definitions(-DGL_GLEXT_PROTOTYPES)
endif()

# define an option for choosing between static and dynamic C runtime (Windows only)
if(SFML_OS_WINDOWS)
    sfml_set_option(SFML_USE_STATIC_STD_LIBS FALSE BOOL "TRUE to statically link to the standard libraries, FALSE to use them as DLLs")

    # the following combination of flags is not valid
    if (BUILD_SHARED_LIBS AND SFML_USE_STATIC_STD_LIBS)
        message(FATAL_ERROR "BUILD_SHARED_LIBS and SFML_USE_STATIC_STD_LIBS cannot be used together")
    endif()

    # for VC++, we can apply it globally by modifying the compiler flags
    if(SFML_COMPILER_MSVC AND SFML_USE_STATIC_STD_LIBS)
        foreach(flag
                CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
                CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
            if(${flag} MATCHES "/MD")
                string(REGEX REPLACE "/MD" "/MT" ${flag} "${${flag}}")
            endif()
        endforeach()
    endif()
endif()

# setup Mac OS X stuff
if(SFML_OS_MACOSX)
    # SFML_BUILD_FRAMEWORKS needs two things:
    # first, it's available only for release
    #    (because cmake currently doesn't allow specifying a custom framework name so XXX-d is not possible)
    # secondly, it works only with BUILD_SHARED_LIBS enabled
    if(SFML_BUILD_FRAMEWORKS)
        # requirement #1
        if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
            message(FATAL_ERROR "CMAKE_BUILD_TYPE should be \"Release\" when SFML_BUILD_FRAMEWORKS is TRUE")
            return()
        endif()

        # requirement #2
        if(NOT BUILD_SHARED_LIBS)
            message(FATAL_ERROR "BUILD_SHARED_LIBS should be TRUE when SFML_BUILD_FRAMEWORKS is TRUE")
            return()
        endif()
    endif()

    # only the default architecture (i.e. 64-bit) is supported
    if(CMAKE_OSX_ARCHITECTURES AND NOT "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64")
        message(FATAL_ERROR "Only 64-bit architecture is supported")
        return()
    endif()
        
    # configure Xcode templates
    set(XCODE_TEMPLATES_ARCH "\$(NATIVE_ARCH_ACTUAL)")
endif()

if(SFML_OS_LINUX OR SFML_OS_FREEBSD)
    set(PKGCONFIG_DIR lib${LIB_SUFFIX}/pkgconfig)
    if(SFML_OS_FREEBSD)
        set(PKGCONFIG_DIR libdata/pkgconfig)
    endif()
    if(BUILD_SHARED_LIBS)
        sfml_set_option(SFML_INSTALL_PKGCONFIG_FILES FALSE BOOL "TRUE to automatically install pkg-config files so other projects can find SFML")
        if(SFML_INSTALL_PKGCONFIG_FILES)
            foreach(sfml_module IN ITEMS all system window graphics audio network)
                CONFIGURE_FILE(
                    "tools/pkg-config/sfml-${sfml_module}.pc.in"
                    "tools/pkg-config/sfml-${sfml_module}.pc"
                    @ONLY)
                INSTALL(FILES "${CMAKE_CURRENT_BINARY_DIR}/tools/pkg-config/sfml-${sfml_module}.pc"
                    DESTINATION "${CMAKE_INSTALL_PREFIX}/${PKGCONFIG_DIR}")
            endforeach()
        endif()
    else()
        if(SFML_INSTALL_PKGCONFIG_FILES)
            message(WARNING "No pkg-config files are provided for the static SFML libraries (SFML_INSTALL_PKGCONFIG_FILES will be ignored).")
        endif()
    endif()
endif()

# enable project folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

# add the subdirectories
add_subdirectory(src/SFML)
if(SFML_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
if(SFML_BUILD_DOC)
    add_subdirectory(doc)
endif()

# setup the install rules
if(NOT SFML_BUILD_FRAMEWORKS)
    install(DIRECTORY include
            DESTINATION .
            COMPONENT devel
            FILES_MATCHING PATTERN "*.hpp" PATTERN "*.inl")

    if(SFML_GENERATE_PDB)
        install(DIRECTORY ${PROJECT_BINARY_DIR}/lib
                DESTINATION .
                COMPONENT devel
                FILES_MATCHING PATTERN "*.pdb")
    endif()
else()
    # find only "root" headers
    file(GLOB SFML_HEADERS RELATIVE ${PROJECT_SOURCE_DIR} "include/SFML/*")

    # in fact we have to fool cmake to copy all the headers in subdirectories
    # to do that we have to add the "root" headers to the PUBLIC_HEADER
    # then we can run a post script to copy the remaining headers

    # we need a dummy file in order to compile the framework
    add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp
                       COMMAND touch ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp)

    set(SFML_SOURCES ${SFML_HEADERS})
    list(APPEND SFML_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/dummy.cpp)

    # create SFML.framework
    add_library(SFML ${SFML_SOURCES})

    # edit target properties
    set_target_properties(SFML PROPERTIES
                          FRAMEWORK TRUE
                          FRAMEWORK_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
                          MACOSX_FRAMEWORK_IDENTIFIER org.sfml-dev.SFML
                          MACOSX_FRAMEWORK_SHORT_VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
                          MACOSX_FRAMEWORK_BUNDLE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
                          PUBLIC_HEADER "${SFML_HEADERS}")

    # add the remaining headers
    add_custom_command(TARGET SFML
                       POST_BUILD
                       COMMAND cp -r ${PROJECT_SOURCE_DIR}/include/SFML/* $<TARGET_FILE_DIR:SFML>/Headers)

    # adapt install directory to allow distributing dylibs/frameworks in user's frameworks/application bundle
    # NOTE: it's not required to link against SFML.framework
    set_target_properties(SFML PROPERTIES
                          BUILD_WITH_INSTALL_RPATH 1
                          INSTALL_NAME_DIR "@rpath")

    # install rule
    install(TARGETS SFML
            FRAMEWORK DESTINATION ${CMAKE_INSTALL_FRAMEWORK_PREFIX}
            COMPONENT devel)
endif()

install(FILES license.txt DESTINATION ${INSTALL_MISC_DIR})
install(FILES readme.txt DESTINATION ${INSTALL_MISC_DIR})
if(NOT SFML_OS_ANDROID)
    install(FILES cmake/Modules/FindSFML.cmake DESTINATION ${INSTALL_MISC_DIR}/cmake/Modules)
endif()

# install 3rd-party libraries and tools
if(SFML_OS_WINDOWS)

    # install the binaries of SFML dependencies
    if(ARCH_32BITS)
        install(DIRECTORY extlibs/bin/x86/ DESTINATION bin)
        if(SFML_COMPILER_MSVC AND SFML_MSVC_VERSION LESS 14)
            install(DIRECTORY extlibs/libs-msvc/x86/ DESTINATION lib)
        elseif(SFML_COMPILER_MSVC)
            install(DIRECTORY extlibs/libs-msvc-universal/x86/ DESTINATION lib)
        else()
            install(DIRECTORY extlibs/libs-mingw/x86/ DESTINATION lib)
        endif()
    elseif(ARCH_64BITS)
        install(DIRECTORY extlibs/bin/x64/ DESTINATION bin)
        if(SFML_COMPILER_MSVC AND SFML_MSVC_VERSION LESS 14)
            install(DIRECTORY extlibs/libs-msvc/x64/ DESTINATION lib)
        elseif(SFML_COMPILER_MSVC)
            install(DIRECTORY extlibs/libs-msvc-universal/x64/ DESTINATION lib)
        else()
            install(DIRECTORY extlibs/libs-mingw/x64/ DESTINATION lib)
        endif()
    endif()

elseif(SFML_OS_MACOSX)

    # install extlibs dependencies only when used
    if("${FLAC_LIBRARY}" STREQUAL "${SFML_SOURCE_DIR}/extlibs/libs-osx/Frameworks/FLAC.framework")
        install(DIRECTORY extlibs/libs-osx/Frameworks/FLAC.framework DESTINATION ${CMAKE_INSTALL_FRAMEWORK_PREFIX})
    endif()

    if("${FREETYPE_LIBRARY}" STREQUAL "${SFML_SOURCE_DIR}/extlibs/libs-osx/Frameworks/freetype.framework")
        install(DIRECTORY extlibs/libs-osx/Frameworks/freetype.framework DESTINATION ${CMAKE_INSTALL_FRAMEWORK_PREFIX})
    endif()

    if("${OGG_LIBRARY}" STREQUAL "${SFML_SOURCE_DIR}/extlibs/libs-osx/Frameworks/ogg.framework")
        install(DIRECTORY extlibs/libs-osx/Frameworks/ogg.framework DESTINATION ${CMAKE_INSTALL_FRAMEWORK_PREFIX})
    endif()

    if("${VORBIS_LIBRARY}" STREQUAL "${SFML_SOURCE_DIR}/extlibs/libs-osx/Frameworks/vorbis.framework")
        install(DIRECTORY extlibs/libs-osx/Frameworks/vorbis.framework DESTINATION ${CMAKE_INSTALL_FRAMEWORK_PREFIX})
    endif()

    if("${VORBISENC_LIBRARY}" STREQUAL "${SFML_SOURCE_DIR}/extlibs/libs-osx/Frameworks/vorbisenc.framework")
        install(DIRECTORY extlibs/libs-osx/Frameworks/vorbisenc.framework DESTINATION ${CMAKE_INSTALL_FRAMEWORK_PREFIX})
    endif()

    if("${VORBISFILE_LIBRARY}" STREQUAL "${SFML_SOURCE_DIR}/extlibs/libs-osx/Frameworks/vorbisfile.framework")
        install(DIRECTORY extlibs/libs-osx/Frameworks/vorbisfile.framework DESTINATION ${CMAKE_INSTALL_FRAMEWORK_PREFIX})
    endif()

    if("${OPENAL_LIBRARY}" STREQUAL "${SFML_SOURCE_DIR}/extlibs/libs-osx/Frameworks/OpenAL.framework")
        install(DIRECTORY "${OPENAL_LIBRARY}" DESTINATION ${CMAKE_INSTALL_FRAMEWORK_PREFIX})
    endif()

    # install the Xcode templates if requested
    if(SFML_INSTALL_XCODE_TEMPLATES)
        # configure the templates plist files
        foreach(TEMPLATE "SFML Compiler" "SFML App")
            configure_file(
                        "tools/xcode/templates/SFML/${TEMPLATE}.xctemplate/TemplateInfo.plist.in"
                        "${CMAKE_CURRENT_BINARY_DIR}/tools/xcode/templates/SFML/${TEMPLATE}.xctemplate/TemplateInfo.plist"
                        @ONLY)
        endforeach()
        install(DIRECTORY "tools/xcode/templates/SFML" "${CMAKE_CURRENT_BINARY_DIR}/tools/xcode/templates/SFML"
                DESTINATION /Library/Developer/Xcode/Templates
                PATTERN "*.in" EXCLUDE
                PATTERN ".DS_Store" EXCLUDE)
    endif()

elseif(SFML_OS_IOS)

    # fix CMake install rules broken for iOS (see http://public.kitware.com/Bug/view.php?id=12506)
    if(SFML_OS_IOS)
        install(DIRECTORY "${CMAKE_BINARY_DIR}/lib/\$ENV{CONFIGURATION}/" DESTINATION lib${LIB_SUFFIX})
    endif()

    # since the iOS libraries are built as static, we must install the SFML dependencies
    # too so that the end user can easily link them to its final application
    install(FILES extlibs/libs-ios/libfreetype.a extlibs/libs-ios/libjpeg.a DESTINATION lib)

elseif(SFML_OS_ANDROID)

    # install extlibs
    install(DIRECTORY extlibs/libs-android/${ANDROID_ABI} DESTINATION extlibs/lib)
    install(FILES extlibs/Android.mk DESTINATION extlibs)

    # install Android.mk so the NDK knows how to set up SFML
    install(FILES src/SFML/Android.mk DESTINATION .)

endif()
