set(SOURCES
    hlslParseHelper.cpp
    hlslScanContext.cpp
    hlslOpMap.cpp
    hlslTokenStream.cpp
    hlslGrammar.cpp)

set(HEADERS
    hlslParseHelper.h
    hlslTokens.h
    hlslScanContext.h
    hlslOpMap.h
    hlslTokenStream.h
    hlslGrammar.h)

add_library(HLSL STATIC ${SOURCES} ${HEADERS})
set_property(TARGET HLSL PROPERTY FOLDER hlsl)

if(WIN32)
    source_group("Source" FILES ${SOURCES} ${HEADERS})
endif(WIN32)

install(TARGETS HLSL
        ARCHIVE DESTINATION lib)
