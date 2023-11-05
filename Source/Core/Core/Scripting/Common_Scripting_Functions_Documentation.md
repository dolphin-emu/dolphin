# Table of Contents

- [About this File](#about)
- [Folder Structure](#folder-structure)
- [File Descriptions](#file-descriptions)
  - [Scripting/CoreScriptInterface/Enums](#core-enums)
  - [Scripting/CoreScriptInterface/InternalScriptAPIs](#core-apis)
  - [Scripting/HelperClasses](#helper-classes)
  - [Scripting/ScriptUtilities.h](#script-utilities)

---

<div id='about'/>

# About this File:

This file provides a description of the various functions/classes which are used to implement Scripting in Dolphin. More specifically, it provides a description of the files that are located in Source/Core/Core/Scripting (starting from the root Dolphin directory). Note that the file which creates the widget/UI for managing scripts is in ScriptManager.h, but I won't discuss that here.

These files are common to all scripts, regardless of the language. To see how to add support for a new scripting language, see the file in this directory called ScriptingPlugins_Documentation.md

<div id='folder-structure'/>

---

# Folder Structure:

The Scripting folder contains 2 files and 2 folders: ScriptUtilities.h, ScriptUtilities.cpp, the HelperClasses folder, and the CoreScriptInterface folder.

The CoreScriptInterface folder is directly copied into the source folder for each DLL, since it defines the common enums and function types that are passed between the DLL and Dolphin. More specifically, the Enums folder in CoreScriptInterface contains all of the enum types that are passed between the DLL and Dolphin, and the InternalScriptAPIs folder inside of CoreScriptInterface contains the definitions for all of the APIs (which is really just a struct of function pointers).

The HelperClasses folder contains the implementations that will be used for the APIs in CoreScriptInterface/InternalScriptAPIs. It also contains various helper classes for creating scripts.

<div id='file-descriptions'/>

---

# File Descriptions:

Here are descriptions of the purpose of each file (ignoring the .cpp files, since those are just implementations of the .h files):

<div id='core-enums'/>

---

## Scripting/CoreScriptInterface/Enums:

### ArgTypeEnum.h:

ArgTypeEnum specifies the types of parameters that can be passed into Scripting functions, and the type of the values returned by Scripting functions.

### GCButtonNameEnum.h:

GCButtonNameEnum specifies an enum value for each button on the GameCube controller. This allows for scripting calls to work that pass in a GameCube button as input.

### ScriptCallLocationsEnum.h:

ScriptCallLocationsEnum specifies where a script was last called from. This is only ever set in Scripting/ScriptUtilities.cpp

### ScriptReturnCodesEnum.h:

ScriptReturnCodesEnum specifies the return code of the last time the script ran. 0 is success, and every other value in the enum is an error code.

<div id='core-apis'/>

---

## Scripting/CoreScriptInterface/InternalScriptAPIs:

### ArgHolder_APIs.h:

Contains the definition of the ArgHolder_APIs struct, which contains a series of function pointers. A struct variable of type ArgHolder_APIs is created by code in Scripting/ScriptUtilities.cpp, which also sets the function pointers of the struct to point to the right definitions. These functions handle creating an ArgHolder pointer (which is passed as a void pointer in order to get past the DLL boundary), setting the value in an ArgHolder, and getting the value in an ArgHolder. Note that ArgHolder is the name of the struct which is used to pass arguments into scripting calls and to return results from scripting calls.

### ClassFunctionsResolver_APIs.h:

Contains the definition of the ClassFunctionsResolver_APIs struct, which contains a series of function pointers. A struct variable of type ClassFunctionsResolver_APIs is created by code in Scripting/ScriptUtilities.cpp, which also sets the function pointers of the struct to point to the right definitions. These functions are called by the DLL, and take a class name or a function name (a string) as input, along with the version number for the class. The functions then send either a ClassMetadata pointer to the DLL, or a FunctionMetadata pointer to the DLL, depending on the function called (in each case, a ScriptContext pointer is passed as input to the function). These functions are the only way that the DLL can get access to a ClassMetadata object or a FunctionMetadata object. The definitions for ClassMetadata, FunctionMetadata and ScriptContext are explained below.

### ClassMetadata_APIs.h:

Contains the definition of the ClassMetadata_APIs struct, which contains a series of function pointers. A struct variable of type ClassMetadata_APIs is created by code in Scripting/ScriptUtilities.cpp, which also sets the function pointers of the struct to point to the right definitions. These functions take as input an opaque handle to a ClassMetadata pointer (which is passed as a void pointer in order to get past the DLL boundary). The ClassMetadata handle is obtained by a call to ClassFunctionsResolver_APIs' Send_ClassMetadataForVersion_To_DLL_Buffer function. The ClassMetadata object contains a name (the name of the class) and a list of FunctionMetadata objects, which represent the functions contained in the class. ClassMetadata_APIs contains the functions to inspect the values contained in the ClassMetadata object.

### FileUtility_APIs.h:

Contains the definition of the FileUtility_APIs struct, which contains a series of function pointers. A struct variable of type FileUtility_APIs is created by code in Scripting/ScriptUtilities.cpp, which also sets the function pointers of the struct to point to the right definitions. These functions let the DLL get the user directory path or the system directory path.

### FunctionMetadata_APIs.h:

Contains the definition of the FunctionMetadata_APIs struct, which contains a series of function pointers. A struct variable of type FunctionMetadata_APIs is created by code in Scripting/ScriptUtilities.cpp, which also sets the function pointers of the struct to point to the right definitions. These functions take as input an opaque handle to a FunctionMetadata pointer (which is passed as a void pointer to get past the DLL boundary). The FunctionMetadata handle is obtained either by making a call to ClassMetadata_APIs' GetFunctionMetadataAtPosition() function on a ClassMetadata pointer, or by calling ClassFunctionsResolver_APIs' Send_FunctionMetadataForVersion_To_DLL_Buffer() function. The FunctionMetadata object contains various information about the Scripting function, along with a pointer to the actual Scripting function. The functions in FunctionMetadata_APIs can be used to obtain the information about the FunctionMetadata, and the FunctionMetadata_APIs' RunFunction() function can be used in order to actually call the function.

### GCButton_APIs.h:

Contains the definition of the GCButton_APIs struct, which contains a series of function pointers. A struct variable of type GCButton_APIs is created by code in Scripting/ScriptUtilities.cpp, which also sets the function pointers of the struct to point to the right definitions. These functions are used to convert a string (representing a button name) into its corresponding GCButtonNameEnum value, or vice versa. It also has functions to say what type of GameCube controller button a given GCButtonNameEnum represents (i.e. either digital or analog).

### ModuleLists_APIs.h:

Contains the definition of the ModuleLists_APIs struct, which contains a series of function pointers. A struct variable of type ModuleLists_APIs is created by code in Scripting/ScriptUtilities.cpp, which also sets the function pointers of the struct to point to the right definitions. These functions return the names of the various modules/classes which are available for scripts to import.

### ScriptContext_APIs.h:

Contains the definitions for the Dolphin_Defined_ScriptContext_APIs struct and the DLL_Defined_ScriptContext_APIs struct. Both of these structs contain a series of function pointers. A struct of type Dolphin_Defined_ScriptContext_APIs is created by code in Scripting/ScriptUtilities.cpp, which also sets the function pointers of the struct to point to the right definitions. The DLL_Defined_ScriptContext_APIs struct, on the other hand, is initialized by the DLL, and is passed as an argument to the Dolphin_Defined_ScriptContext_APIs' ScriptContext_Initializer() function (which is then stored as a variable inside of the ScriptContext object). The Dolphin_Defined_ScriptContext_APIs contains functions which all Scripts have in common, such as the function to create a new ScriptContext. The DLL_Defined_ScriptContext_APIs on the other hand stores functions whose implementation varies depending on the scripting language.

### VectorOfArgHolders_APIs.h:

Contains the definition of the VectorOfArgHolders_APIs struct, which contains a series of function pointers. A struct of type VectorOfArgHolders_APIs is created by the code in Scripting/ScriptUtilities.cpp, which also sets the function pointers of the struct to point to the right definitions. These functions create a VectorOfArgHolders object (returned as a void pointer), and contain functions to manipulate a VectorOfArgHolders. VectorOfArgHolders is what actually holds the ArgHolders that are passed as arguments to a function. The VectorOfArgHolders_APIs struct also contains functions to get the ArgHolder pointer stored at the specified index in the vector.

---

<div id='helper-classes'/>

##  Scripting/HelperClasses:

### ArgHolder.h:

Contains the definition for the ArgHolder struct. This is what holds each of the arguments passed into a Scripting function. Additionally, each Scripting function returns an ArgHolder pointer. More specifically, the signature for each Scripting function is as follows:

```
ArgHolder* funcName(ScriptContext* current_script, std::vector<ArgHolder*>* args_list);
```

### ArgHolder_API_Implementations.h:

Contains the implementations for the functions specified in ArgHolder_APIs.h.

### ClassFunctionsResolver_API_Implementations.h:

Contains the implementations for the functions specified in ClassFunctionsResolver_APIs.h.

### ClassMetadata.h:

Contains the definition for the ClassMetadata struct, and the implementations of the functions specified in ClassMetadata_APIs.h. The ClassMetadata struct holds the name of the class/module, and a list of FunctionMetadata pointers, which represent the functions available in the class for the version specified by the user in the call to ClassFunctionsResolver_APIs' Send_ClassMetadataForVersion_To_DLL_Buffer() function (the only way for the DLL to get an opaque handle to a ClassMetadata pointer).

### FileUtility_API_Implementations.h:

Contains the implementations for the functions specified in FileUtility_APIs.h.

### FunctionMetadata.h:

Contains the definition for the FunctionMetadata struct, and the implementations of the functions specified in FunctionMetadata_APIs.h. The FunctionMetadata struct holds the name of the function, the number/type of each argument that it takes as input, the return type of the function, and a pointer to the actual function definition. The DLL can get an opaque handle to a FunctionMetadata pointer by calling ClassMetadata_APIs' GetFunctionMetadataAtPosition() function on a ClassMetadata handle, or ClassFunctionsResolver_APIs' Send_FunctionMetadataForVersion_To_DLL_Buffer() function.

### GCButton_API_Implementations.h:

Contains the implementations for the functions specified in GCButton_APIs.h.

### InstructionBreakpointsHolder.h:

Contains the definition for the InstructionBreakpointsHolder struct, which is used to store all of the instruction address breakpoints for a given script. A variable of type InstructionBreakpointsHolder is stored inside of the ScriptContext class.

### MemoryAddressBreakpointsHolder.h:

Contains the definition for the MemoryAddressBreakpointsHolder struct, which is used to store all of the memory-read and memory-write breakpoints for a given script. A variable of type MemoryAddressBreakpointsHolder is stored inside of the ScriptContext class.

### ModuleLists_API_Implementations.h:

Contains the implementations for the functions specified in ModuleLists_APIs.h.

### ScriptContext.h:

Contains the definition for the ScriptContext struct. This is the main struct which is used to represent a script (each script is represented by one and only one ScriptContext object). Furthermore, this is what's returned by a call to Dolphin_Defined_ScriptContext_APIs' ScriptContext_Initializer() function (and by the DLL's CreateNewScript() function). This class contains various variables that are common to all scripts, regardless of language/DLL, such as the filename of the script.

### VectorOfArgHolders_API_Implementations.h:

Contains the implementations for the functions specified in VectorOfArgHolders_APIs.h. The CreateNewVectorOfArgHolders_impl() function creates a std::vector<ArgHolder*>*, and returns it as a void pointer/opaque handle. The other functions in this class take that opaque handle as input.


### VersionComparisonFunctions.h:

This is a helper file that contains functions which are used by VersionResolver.h to determine which of 2 version strings is greater than the other (or to determine which version is greater than or equal to the other one).

### VersionResolver.h:

Contains the functions to get all functions for a given version of a class. In order for a function to be included in the return-result, the version must be >= the version that the function was first defined in. Additionally, the function definition cannot appear in the deprecated_functions map with a version number <= the current version number. Versions are compared with the assumption that "1.0" is the earliest version of a function. Additionally, each number in a version between decimal points is compared in order to the corresponding number in the other version from left-to-right, until one number is greater than the other (in order to determine which version is greater). If the 2 versions have a different number of decimal points, then the one with less decimal points has extra .0s added until it has the same number of decimal points as the 1st version. For example, suppose we're comparing version "3.12" and version "3.12.4". In this case, version "3.12.4" would be greater, since we would convert version "3.12" into version "3.12.0", and the 4 after the 2nd decimal point in "3.12.4" is greater than the 0 after the 2nd decimal point in "3.12.0"

---

<div id='script-utilities'/>

## Scripting/ScriptUtilities.h:

ScriptUtilities.h is the main Scripting file that the rest of Dolphin interacts with when a file needs to create/run a script. It contains the function to create a script, the function to start a script, and the functions to run various script callbacks, which include OnFrameStartCallbacks, OnGCInputPolledCallbacks, OnWiiInputPolledCallbacks, OnInstructionHitCallbacks, OnMemoryAddressReadFromCallbacks, and OnMemoryAddressWrittenToCallbacks. This file is also responsible for creating a series of static API structs when a Script is first created, and setting the function pointers in this struct to point to the corresponding definitions. The file is also responsible for reading through the PluginSources folder to initialize all the DLLs that are defined in that folder.

A large chunk of this file has been replaced with TODO statements, for simplicity-sake. If you would like to see what the finished version of this file would look like, you can check this out: https://github.com/Lobsterzelda/lobster-zelda-dolphin/blob/lua-support/Source/Core/Core/Scripting/ScriptUtilities.cpp

Note that the function calls to run a script at the start of each frame are located at the start of the Core::Callback_NewField() function (in Core.cpp), which calls the corresponding functions in ScriptUtilities.h.

---
