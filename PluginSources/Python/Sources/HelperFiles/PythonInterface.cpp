#include "PythonInterface.h"
#include "Python.h"

#include <string>

typedef struct {
  PyObject_HEAD
    FunctionMetadata* function_metadata_ptr;
} PythonFunctionMetadataCallObject;

typedef struct {
  PyObject_HEAD
    std::string class_name;
} PythonClassMetadataObject;


static PyObject* RunFunction(PyObject* self, PyObject* args)
{
  PythonFunctionMetadataCallObject* casted_obj = (PythonFunctionMetadataCallObject*)self;
  return nullptr;
}

static PyObject* function_call(PyObject* self, PyObject* args, PyObject* kwargs)
{
  return RunFunction(self, args);
}

static PyTypeObject MyObject_Type = {
    .ob_base = PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "FunctionMetadataObject",
    .tp_basicsize = sizeof(PythonFunctionMetadataCallObject),
    .tp_call = function_call
};

void Python_IncRef(void* x) {
}
void Python_DecRef(void* x) {}
void* GetNoneObject(void* x) { return nullptr; }
void* GetPyTrueObject() { return nullptr; }
void* GetPyFalseObject() { return nullptr; }
void* Python_BuildValue(const char* x, void* y) { return nullptr; }
void* Py_RunString(const char* x) { return nullptr; }
void* Py_RunAnyFile(const char* x) { return nullptr; }
void Python_SetRunTimeError(const char* x) {}
bool Python_ErrOccured() { return true; }
void Python_Initialize() {}
void* PythonThreadState_Get() { return nullptr; }
void PythonEval_RestoreThread(void* x) {}
void PythonEval_ReleaseThread(void* x) {}
void* Python_NewInterpreter() { return nullptr; }

void* CreatePythonModuleFromClassMetadata(ClassMetadata* class_metadata_ptr)
{
  
}

void* PythonObject_CallFunction(void* x) { return nullptr; }

bool PythonObject_IsTrue(void* x) { return true; }
int PythonTuple_GetSize(void* x) { return 0; }
void* PythonTuple_GetItem(void* x, unsigned long long y) { return nullptr; }
unsigned long long PythonLongObj_AsU64(void* x) { return 8; }
signed long long PythonLongObj_AsS64(void* x) { return 4; }
double PythonFloatObj_AsDouble(void* x) { return 3; }
const char* PythonUnicodeObj_AsString(void* x) { return ""; }
void* PythonDictionary_New() { return nullptr; }
void PythonDictionary_SetItem(void* x, signed long long y, signed long long z) {}
bool PythonDict_Next(void* x, unsigned long long y, void* z, void* u) { return true; }
unsigned long long PythonList_Size(void* x) { return 3; }
void* PythonList_GetItem(void* x, unsigned long long y) { return nullptr; }
bool PythonList_Check(void* x) { return true; }
bool PythonTuple_Check(void* x) { return true; }
void* StringTo_PythonUnicodeObj(const char* x) { return nullptr; }
bool PythonBooleanObj_FromLong(long long x) { return true; }
