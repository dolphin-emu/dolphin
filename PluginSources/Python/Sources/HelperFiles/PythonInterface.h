#ifndef PYTHON_INTERFACE
#define PYTHON_INTERFACE

#include <string>
#include "ClassMetadata.h"

#ifdef __cplusplus
extern  "C" {
#endif

  namespace PythonInterface
  {
    // This file encapsulates all Python DLL operations into one file (all void* are really PyObject*)
    void Python_IncRef(void*);
    void Python_DecRef(void*);
    void* GetNoneObject();
    void* GetPyTrueObject();
    void* GetPyFalseObject();
    void* Python_BuildValue(const char*, void*);
    void Python_RunString(const char*);
    void Python_RunFile(const char*);
    void Python_SetRunTimeError(const char*);
    bool Python_ErrOccured();
    void Python_CallPyErrPrintEx();
    void Python_SendOutputToCallbackLocationAndClear(void*, void (*print_callback)(void*, const char*));
    void Python_Initialize();
    void* PythonThreadState_Get();
    void PythonEval_RestoreThread(void*);
    void PythonEval_ReleaseThread(void*);
    void* Python_NewInterpreter();

    void CreateThisModule(); // Creates an empty THIS module, which will store the PythonScriptContext*. This is called before Py_Initialize.
    bool SetThisModule(void*); // Sets the THIS module to contain the specified PythonScriptContext*. This is called after Py_Initialize.
    void CreateEmptyModule(const char*); // Creates an empty module with the specified name - called for every module before Py_Initialize.
    bool SetModuleVersion(const char*, const char*); // Sets the module (the 1st arg) to have the version specified by the 2nd arg. Called for every module after Py_Initialize. Returns true if the attempt to set the module version succeeds, and false otherwise.
    bool SetModuleFunctions(const char*, ClassMetadata*, std::vector<void*>*); // Sets the module (the 1st arg) to have the functions specified by the 2nd arg. Called for all default modules after Py_Initialize, and called for non-default modules at the point where they're imported (only if they're imported). 3rd arg stores PyMethodDefs, which are deleted when the object is deallocated.
    void DeleteMethodDefsVector(std::vector<void*>*); // Called when a PythonScriptContext* is terminating to delete the allocated PyMethodDefs
    const char* GetModuleVersion(const char*);

    bool RunImportCommand(const char*); // Imports the module specified by the argument. Called by all default modules after Py_Initialize, and called by non-default modules at the point where they're imported (only if they're imported).

    void AppendArgumentsToPath(const char*, const char*);
    void RedirectStdOutAndStdErr();

    bool Python_IsCallable(void*);
    void* PythonObject_CallFunction(void*);

    bool PythonObject_IsTrue(void*);
    unsigned long long PythonTuple_GetSize(void*);
    void* PythonTuple_GetItem(void*, unsigned long long);
    bool PythonTuple_Check(void*);
    unsigned long long PythonLongObj_AsU64(void*);
    signed long long PythonLongObj_AsS64(void*);
    void* S64_ToPythonLongObj(signed long long);
    void* U64_ToPythonLongObj(unsigned long long);
    double PythonFloatObj_AsDouble(void*);
    const char* PythonUnicodeObj_AsString(void*);
    void* PythonDictionary_New();
    void PythonDictionary_SetItem(void*, void*, void*);
    void* ResetAndGetRef_ToPyKey();
    void* ResetAndGetRef_ToPyVal();
    bool PythonDict_Next(void*, signed long long*, void**, void**);
    unsigned long long PythonList_Size(void*);
    void* PythonList_GetItem(void*, unsigned long long);
    bool PythonList_Check(void*);
    void* StringTo_PythonUnicodeObj(const char*);
    void* PythonBooleanObj_FromLong(long long);


  }
#ifdef __cplusplus
}
#endif

#endif
