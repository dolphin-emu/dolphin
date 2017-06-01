
set(INCROOT ${PROJECT_SOURCE_DIR}/include/SFML/Main)
set(SRCROOT ${PROJECT_SOURCE_DIR}/src/SFML/Main)

# sources
if(SFML_OS_WINDOWS)
    set(SRC ${SRC} ${SRCROOT}/MainWin32.cpp)
elseif(SFML_OS_IOS)
    set(SRC ${SRC} ${SRCROOT}/MainiOS.mm)
elseif(SFML_OS_ANDROID)
    set(SRC ${SRC} ${SRCROOT}/MainAndroid.cpp)
else()
    return()
endif()
source_group("" FILES ${SRC})

# define the sfml-main target
add_library(sfml-main STATIC ${SRC})

# set the debug suffix
set_target_properties(sfml-main PROPERTIES DEBUG_POSTFIX -d)

# insert the major version number in the output filename
set_target_properties(sfml-main PROPERTIES OUTPUT_NAME "sfml-main")

# set the target's folder (for IDEs that support it, e.g. Visual Studio)
set_target_properties(sfml-main PROPERTIES FOLDER "SFML")

# setup the install rule
install(TARGETS sfml-main ARCHIVE DESTINATION lib${LIB_SUFFIX} COMPONENT devel)

# because of a current limitation on Android (which prevents one library
# from depending on shared libraries), we need a boostrap activity which
# will load our shared libraries manually
if(SFML_OS_ANDROID)
    sfml_add_library(sfml-activity SOURCES ${PROJECT_SOURCE_DIR}/src/SFML/Main/SFMLActivity.cpp)
endif()
