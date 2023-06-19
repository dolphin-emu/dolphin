cmake_minimum_required(VERSION 3.27)
set(CMAKE_CXX_FLAGS)

project(LuaDLL)

add_subdirectory(LuaExternals)

add_library(LUA SHARED
CopiedScriptContextFiles/Enums/ArgTypeEnum.h
CopiedScriptContextFiles/Enums/GCButtonNameEnum.h
CopiedScriptContextFiles/Enums/ScriptCallLocations.h
CopiedScriptContextFiles/InternalScriptAPIs/ArgHolder_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/ClassFunctionsResolver_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/ClassMetadata_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/FileUtility_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/FunctionMetadata_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/GCButton_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/ScriptContext_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/VectorOfArgHolders_APIs.h
LuaScriptContextFiles/LuaScriptContext.h
LuaScriptContextFiles/LuaScriptContext.cpp
CreateScript.h
CreateScript.cpp
DolphinDefinedAPIs.h
DolphinDefinedAPIs.cpp
HelperFiles/ClassMetadata.h
HelperFiles/FunctionMetadata.h
)

set_target_properties(LUA PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(LUA PUBLIC LuaExternals)

set_target_properties(LUA PROPERTIES 
 CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../.. 
 CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/../..
 CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../..
)

set_target_properties(LUA
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/../..
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/../..
)