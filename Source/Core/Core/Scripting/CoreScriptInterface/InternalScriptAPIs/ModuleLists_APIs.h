#pragma once

namespace Scripting
{

#ifdef __cplusplus
extern "C" {
#endif

// ModuleLists_APIs contains the APIs to get the list of available modules that are defined in the
// Dolphin source code for Scripting APIs.
typedef struct ModuleLists_APIs
{
  // Returns an opaque handle to the list of all module names which should be imported by default
  // when a script starts (without the user needing to manually import them - ex."OnInstructionHit")
  const void* (*GetListOfDefaultModules)();

  // Returns an opaque handle to the list of all module names which are NOT imported by default
  // when a script starts (ones that users need to import manually in order to use - ex. "BitAPI")
  const void* (*GetListOfNonDefaultModules)();

  // Takes as input an opaque handle to a list returned by one of the first 2 functions of this
  // struct, and returns the size of the list.
  unsigned long long (*GetSizeOfList)(const void*);

  // Takes as input an opaque handle to a list returned by one of the first 2 functions of this
  // struct, and returns the module name at the specified index in the list (numbered starting from
  // 0)
  const char* (*GetElementAtListIndex)(const void*, unsigned long long);

  // Returns the name of the module which contains the "import" and "importModule" functions.
  const char* (*GetImportModuleName)();

} ModuleLists_APIs;

#ifdef __cplusplus
}
#endif

}  // namespace Scripting
