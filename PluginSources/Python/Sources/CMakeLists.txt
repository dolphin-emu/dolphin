cmake_minimum_required(VERSION 3.27)
set(CMAKE_CXX_FLAGS)

project(PythonDLL)


add_library(PYTHON SHARED
CopiedScriptContextFiles/Enums/ArgTypeEnum.h
CopiedScriptContextFiles/Enums/GCButtonNameEnum.h
CopiedScriptContextFiles/Enums/ScriptCallLocations.h
CopiedScriptContextFiles/Enums/ScriptReturnCodes.h
CopiedScriptContextFiles/InternalScriptAPIs/ArgHolder_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/ClassFunctionsResolver_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/ClassMetadata_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/FileUtility_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/FunctionMetadata_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/GCButton_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/ModuleLists_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/ScriptContext_APIs.h
CopiedScriptContextFiles/InternalScriptAPIs/VectorOfArgHolders_APIs.h
PythonScriptContextFiles/PythonScriptContext.h
PythonScriptContextFiles/PythonScriptContext.cpp
CreateScript.h
CreateScript.cpp
DolphinDefinedAPIs.h
DolphinDefinedAPIs.cpp
HelperFiles/ClassMetadata.h
HelperFiles/FunctionMetadata.h
HelperFiles/DynamicLibrary.h
HelperFiles/DynamicLibrary.cpp
HelperFiles/IdentifierToCallback.h
HelperFiles/MemoryAddressCallbackTriple.h
HelperFiles/PythonDynamicLibrary.h
HelperFiles/PythonDynamicLibrary.cpp
HelperFiles/PythonInterface.h
HelperFiles/PythonInterface.cpp
)

set_target_properties(PYTHON PROPERTIES LINKER_LANGUAGE CXX)
set_property(TARGET PYTHON PROPERTY CXX_STANDARD 20)

set_target_properties(PYTHON PROPERTIES 
 CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../../../../../Data/Sys/ScriptingPlugins/Python
 CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/../../../../../Data/Sys/ScriptingPlugins/Python
 CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../../../../../Data/Sys/ScriptingPlugins/Python
)

set_target_properties(PYTHON
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/../../../../../Data/Sys/ScriptingPlugins/Python
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/../../../../../Data/Sys/ScriptingPlugins/Python
)