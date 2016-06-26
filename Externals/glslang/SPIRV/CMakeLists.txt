set(SOURCES
    GlslangToSpv.cpp
    InReadableOrder.cpp
    Logger.cpp
    SpvBuilder.cpp
    SPVRemapper.cpp
    doc.cpp
    disassemble.cpp)

set(HEADERS
    spirv.hpp
    GLSL.std.450.h
    GlslangToSpv.h
    Logger.h
    SpvBuilder.h
    SPVRemapper.h
    spvIR.h
    doc.h
    disassemble.h)

add_library(SPIRV STATIC ${SOURCES} ${HEADERS})
set_property(TARGET SPIRV PROPERTY FOLDER glslang)

if(WIN32)
    source_group("Source" FILES ${SOURCES} ${HEADERS})
endif(WIN32)

install(TARGETS SPIRV
        ARCHIVE DESTINATION lib)
