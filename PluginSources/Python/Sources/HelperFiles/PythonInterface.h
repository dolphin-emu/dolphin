#ifndef PYTHON_INTERFACE
#define PYTHON_INTERFACE

#include <string>
#include "ClassMetadata.h"

#ifdef __cplusplus
extern  "C" {
#endif

// This file encapsulates all Python DLL operations into one file (all void* are really PyObject*)
void Python_IncRef(void*);
void Python_DecRef(void*);
void* GetNoneObject(void*);
void* GetPyTrueObject();
void* GetPyFalseObject();
void* Python_BuildValue(const char*, void*);
void* Py_RunString(const char*);
void* Py_RunAnyFile(const char*);
void Python_SetRunTimeError(const char*);
bool Python_ErrOccured();
void Python_Initialize();
void* PythonThreadState_Get();
void PythonEval_RestoreThread(void*);
void PythonEval_ReleaseThread(void*);
void* Python_NewInterpreter();

void* CreatePythonModuleFromClassMetadata(ClassMetadata*);
void* PythonObject_CallFunction(void*);

bool PythonObject_IsTrue(void*);
int PythonTuple_GetSize(void*);
void* PythonTuple_GetItem(void*, unsigned long long);
unsigned long long PythonLongObj_AsU64(void*);
signed long long PythonLongObj_AsS64(void*);
double PythonFloatObj_AsDouble(void*);
const char* PythonUnicodeObj_AsString(void*);
void* PythonDictionary_New();
void PythonDictionary_SetItem(void*, signed long long, signed long long);
bool PythonDict_Next(void*, unsigned long long, void*, void*);
unsigned long long PythonList_Size(void*);
void* PythonList_GetItem(void*, unsigned long long);
bool PythonList_Check(void*);
bool PythonTuple_Check(void*);
void* StringTo_PythonUnicodeObj(const char*);
bool PythonBooleanObj_FromLong(long long);



#ifdef __cplusplus
}
#endif

#endif
