# Table of Contents

- [About this File](#about)
- [Folder Structure for Scripting Plugins](#folder-structure)
- [Functions that Scripting Plugins Need to Export as DLL Functions](#required-functions)
- [Functions that Scripting Plugins Need to Implement](#implemented-functions)
- [Creating the DLL](#creation)

---

<div id='about'/>

# About this File:

This file specifies how to create a Scripting Plugin (what files are required to create a Scripting Plugin, what functions need to be created/defined, etc.). To see an example of a completed ScriptingPlugin for Lua, you can check out this link: https://github.com/Lobsterzelda/lobster-zelda-dolphin/tree/lua-support/PluginSources/Lua

If you want to read a description of what each file does which is located in the Source/Core/Core/Scripting folder (which is shared by all DLLs), then you can check out the file in this folder called Common_Scripting_Functions_Documentation.md

---

<div id='folder-structure'/>

# Folder Structure for Scripting Plugins:

All Scripting plugins will get put into the PluginSources folder, which will be put in the root directory of Dolphin (the same folder that contains the Binary folder).

Each language will be contained in a separate folder inside of PluginSources - to reduce ambiguity, this should be the name of the language that's being implemented.

The language folder will contain a Source folder, which contains all of the source code for the plugin. The language folder will also contain a file called extensions.txt, which lists the extension types for that language. If we want all scripts ending in .lua to be treated as Lua scripts, then extensions.txt should just contain the following line:

.lua

As an example, suppose we want to implement the Lua language. Then, our file structure will look like this, with the root directory representing the root directory for Dolphin (D represents directories, F represents files):

/ (D)

----PluginSources (D)

--------Lua (D)

------------extensions.txt (F)

------------Source (D)

----------------CoreScriptInterface (D)

----------------<The source code for the DLL> (Multiple files and or directories).

Note that the CoreScriptInterface folder is an exact copy of the folder located at Source/Core/Scripting/CoreScriptInterface (it must be copied over into each DLL).


<div id = 'required-functions'/>

---

# Functions That Scripting Plugins Need To Export as DLL Functions:

There are 10 functions that each Scripting Plugin must implement and export. Here is the formatting to do that:

```
#ifdef WIN32
	#define DLL_Export   __declspec( dllexport )
#else
	#define DLL_Export
#endif

DLL_Export void Init_ArgHolder_APIs(ArgHolder_APIs*);
DLL_Export void Init_ClassFunctionsResolver_APIs(ClassFunctionsResolver_APIs*);
DLL_Export void Init_ClassMetadata_APIs(ClassMetadata_APIs*);
DLL_Export void Init_FileUtility_APIs(FileUtility_APIs*);
DLL_Export void Init_FunctionMetadata_APIs(FunctionMetadata_APIs*);
DLL_Export void Init_GCButton_APIs(GCButton_APIs*);
DLL_Export void Init_ModuleLists_APIs(ModuleLists_APIs*);
DLL_Export void Init_ScriptContext_APIs(Dolphin_Defined_ScriptContext_APIs*);
DLL_Export void Init_VectorOfArgHolders_APIs(VectorOfArgHolders_APIs*);

DLL_Export void* CreateNewScript(int, const char*, Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE, Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE);
```

The definitions for the various parameters in these functions can be found in the CoreScriptInterface/InternalScriptAPIs folder (each will be in the file with the same name as the type, with the exception of Dolphin_Defined_ScriptContext_APIs, which is in the ScriptContext_APIs.h file).

The first 9 of these functions are self-explanatory. They just initialize the various APIs for the DLL to have the right values. Scripts should have a static variable of type FileUtility_APIs (or whatever type is referenced by the function), which will store the function pointers for the APIs. These functions should only be called once, when a DLL is initialized for the first time. These functions will generally have a one-line definition. For example, here is a possible definition for the Init_ArgHolder_APIs function.

```
ArgHolder_APIs argHolder_APIs;

DLL_Export void Init_ArgHolder_APIs(ArgHolder_APIs* new_argHolder_APIs)
{
  argHolder_APIs = *new_argHolder_APIs;
}
```

The last function, CreateNewScript(), is called whenever we want to create a new script of the specified type. It should call the dolphinDefinedScriptContext_APIs.ScriptContext_Initializer() function, and it should return the result of calling this function. The dolphinDefinedScriptContext_APIs variable should be set with a call to Init_ScriptContext_APIs by Dolphin before trying to create a script for the first time. The CreateNewScript() function should also take care of whatever DLL-specific initializations need to happen (which should happen AFTER calling ScriptContext_Initializer()).

---

<div id='implemented-functions'/>

# Functions that Scripting Plugins Need to Implement:

The Scripting Plugin must create a struct of type dll_defined_scriptContext_apis (defined in CoreScriptInterface/InternalScriptAPIs/ScriptContext_APIs). This struct is just a list of function pointers, so after the Scripting Plugin creates this struct, it must contain set each function pointer in the struct to point to the implementing function. In other words, all functions referenced by this struct must be implemented by the DLL. Here is some example code that could implement these functions (assuming that the implementation functions are defined somewhere by the DLL):

```
static DLL_Defined_ScriptContext_APIs dll_defined_scriptContext_apis = {};
static bool initialized_dll_specific_script_context_apis = false;

static void InitDLLSpecificAPIs()
{
  dll_defined_scriptContext_apis.DLLClassMetadataCopyHook = DLLClassMetadataCopyHook_impl;
  dll_defined_scriptContext_apis.DLLFunctionMetadataCopyHook = DLLFunctionMetadataCopyHook_impl;
  dll_defined_scriptContext_apis.DLLSpecificDestructor = Destroy_LuaScriptContext_impl;
  dll_defined_scriptContext_apis.GetButtonCallbackAndAddToQueue = GetButtonCallbackAndAddToQueue_impl;
  dll_defined_scriptContext_apis.ImportModule = ImportModule_impl;
  dll_defined_scriptContext_apis.IsButtonRegistered = IsButtonRegistered_impl;
  dll_defined_scriptContext_apis.RegisterButtonCallback = RegisterButtonCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnFrameStartCallback = RegisterOnFrameStartCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnFrameStartWithAutoDeregistrationCallback = RegisterOnFrameStartWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnGCControllerPolledCallback = RegisterOnGCControllerPolledCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnGCControllerPolledWithAutoDeregistrationCallback = RegisterOnGCControllerPolledWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnInstructionReachedCallback = RegisterOnInstructionReachedCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnInstructionReachedWithAutoDeregistrationCallback = RegisterOnInstructionReachedWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnMemoryAddressReadFromCallback = RegisterOnMemoryAddressReadFromCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback = RegisterOnMemoryAddressReadFromWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnMemoryAddressWrittenToCallback = RegisterOnMemoryAddressWrittenToCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback = RegisterOnMemoryAddressWrittenToWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnWiiInputPolledCallback = RegisterOnWiiInputPolledCallback_impl;
  dll_defined_scriptContext_apis.RegisterOnWiiInputPolledWithAutoDeregistrationCallback = RegisterOnWiiInputPolledWithAutoDeregistrationCallback_impl;
  dll_defined_scriptContext_apis.RunButtonCallbacksInQueue = RunButtonCallbacksInQueue_impl;
  dll_defined_scriptContext_apis.RunGlobalScopeCode = RunGlobalScopeCode_impl;
  dll_defined_scriptContext_apis.RunOnFrameStartCallbacks = RunOnFrameStartCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnGCControllerPolledCallbacks = RunOnGCControllerPolledCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnInstructionReachedCallbacks = RunOnInstructionReachedCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnMemoryAddressReadFromCallbacks = RunOnMemoryAddressReadFromCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnMemoryAddressWrittenToCallbacks = RunOnMemoryAddressWrittenToCallbacks_impl;
  dll_defined_scriptContext_apis.RunOnWiiInputPolledCallbacks = RunOnWiiInputPolledCallbacks_impl;
  dll_defined_scriptContext_apis.StartScript = StartScript_impl;
  dll_defined_scriptContext_apis.UnregisterOnFrameStartCallback = UnregisterOnFrameStartCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnGCControllerPolledCallback = UnregisterOnGCControllerPolledCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnInstructionReachedCallback = UnregisterOnInstructionReachedCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnMemoryAddressReadFromCallback = UnregisterOnMemoryAddressReadFromCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnMemoryAddressWrittenToCallback = UnregisterOnMemoryAddressWrittenToCallback_impl;
  dll_defined_scriptContext_apis.UnregisterOnWiiInputPolledCallback = UnregisterOnWiiInputPolledCallback_impl;
  initialized_dll_specific_script_context_apis = true;
}
```

This code need only be run once, when an instance of a Script for a given language is created for the first time (when CreateNewScript() is called for the first time for that DLL).

A pointer to this DLL is passed to the dolphinDefinedScriptContext_APIs.ScriptContext_Initializer() function. Taken as a whole, the CreateNewScript() function for the Lua DLL might look something like this:

```
DLL_Export void* CreateNewScript(int unique_script_identifier, const char* script_filename, Dolphin_Defined_ScriptContext_APIs::PRINT_CALLBACK_FUNCTION_TYPE print_callback_fn_ptr, Dolphin_Defined_ScriptContext_APIs::SCRIPT_END_CALLBACK_FUNCTION_TYPE script_end_callback_fn_ptr)
{
  if (!initialized_dll_specific_script_context_apis)
    InitDLLSpecificAPIs();

  void* opaque_script_context_handle = dolphinDefinedScriptContext_APIs.ScriptContext_Initializer(unique_script_identifier, script_filename, print_callback_fn_ptr, script_end_callback_fn_ptr, &dll_defined_scriptContext_apis);
  LuaScriptContext* new_lua_script_context_ptr = reinterpret_cast<LuaScriptContext*>(Init_LuaScriptContext_impl(opaque_script_context_handle));
  return opaque_script_context_handle;
```

Init_LuaScriptContext_impl doesn't need to exist - it's just a custom function I wrote which creates a LuaScriptContext pointer (also a custom type I made) in order to handle the fields specific to a Lua script, while also containing a pointer to the ScriptContext pointer which is returned from the dolphinDefinedScriptContext_APIs.ScriptContext_Initializer() function.

---

<div id='creation'/>

# Creating the DLL:

All of the scripting files from a Scripting Plugin need to be compiled into a DLL, in order for scripts to be written in that language. This can be done by creating a CMakeLists.txt file, specifying the library type as shared (in order to make it a DLL project), and then compiling the CMakeLists.txt file in Visual Studio. The CMakeLists.txt file should copy the resulting DLL to a folder located in Data/Sys/ScriptingPlugins/<LanguageName>, in order for the DLL to be used at runtime.

Below is an example for what CMakeLists.txt could look like for Lua (this would be located in the "Source" folder for Lua that was referenced in the "Folder Structure" section of this page:

```
cmake_minimum_required(VERSION 3.27)
set(CMAKE_CXX_FLAGS)

project(LuaDLL)

add_subdirectory(LuaExternals)

add_library(LUA SHARED
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
LuaScriptContextFiles/LuaScriptContext.h
LuaScriptContextFiles/LuaScriptContext.cpp
CreateScript.h
CreateScript.cpp
DolphinDefinedAPIs.h
DolphinDefinedAPIs.cpp
HelperFiles/ClassMetadata.h
HelperFiles/FunctionMetadata.h
HelperFiles/MemoryAddressCallbackTriple.h
)

set_target_properties(LUA PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(LUA PUBLIC LuaExternals)

set_target_properties(LUA PROPERTIES 
 CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../../../../../Data/Sys/ScriptingPlugins/Lua
 CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/../../../../../Data/Sys/ScriptingPlugins/Lua
 CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/../../../../../Data/Sys/ScriptingPlugins/Lua
)

set_target_properties(LUA
    PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/../../../../../Data/Sys/ScriptingPlugins/Lua
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/$<CONFIG>/../../../../../Data/Sys/ScriptingPlugins/Lua
)
```

Note that LuaExternals would be a folder in the Source folder which contains the Lua source code (downloaded from the Lua website).

---
