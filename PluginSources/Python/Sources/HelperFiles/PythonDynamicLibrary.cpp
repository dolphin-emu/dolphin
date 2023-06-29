#include "PythonDynamicLibrary.h"
#include "../DolphinDefinedAPIs.h"
#include "../CopiedScriptContextFiles/Enums/ScriptReturnCodes.h"
#include <memory>
#include <string>
#include <vector>

DynamicLibrary* python_lib_ptr = nullptr;
static std::string path_to_lib = "";

static const char* PY_BOOL_FROM_LONG_FUNC_NAME = "PyBool_FromLong";
static const char* PY_BUILD_VALUE_FUNC_NAME = "Py_BuildValue";
static const char* PY_CALLABLE_CHECK_FUNC_NAME = "PyCallable_Check";
static const char* PY_CAPSULE_GET_POINTER_FUNC_NAME = "PyCapsule_GetPointer";
static const char* PY_CAPSULE_NEW_FUNC_NAME = "PyCapsule_New";
static const char* PY_CMETHOD_NEW = "PyCMethod_New";
static const char* PY_DEC_REF_FUNC_NAME = "Py_DecRef";
static const char* PY_DICT_NEW_FUNC_NAME = "PyDict_New";
static const char* PY_DICT_NEXT_FUNC_NAME = "PyDict_Next";
static const char* PY_DICT_SET_ITEM_FUNC_NAME = "PyDict_SetItem";
static const char* PY_DICT_SET_ITEM_STRING_FUNC_NAME = "PyDict_SetItemString";
static const char* PY_ERR_OCCURRED_FUNC_NAME = "PyErr_Occurred";
static const char* PY_ERR_PRINT_EX_FUNC_NAME = "PyErr_PrintEx";
static const char* PY_ERR_SET_STRING_FUNC_NAME = "PyErr_SetString";
static const char* PY_EVAL_RELEASE_THREAD_FUNC_NAME = "PyEval_ReleaseThread";
static const char* PY_EVAL_RESTORE_THREAD_FUNC_NAME = "PyEval_RestoreThread";
static const char* PY_FLOAT_AS_DOUBLE_FUNC_NAME = "PyFloat_AsDouble";
static const char* PY_IMPORT_APPEND_INIT_TAB_FUNC_NAME = "PyImport_AppendInittab";
static const char* PY_IMPORT_IMPORT_MODULE_FUNC_NAME = "PyImport_ImportModule";
static const char* PY_INC_REF_FUNC_NAME = "Py_IncRef";
static const char* PY_INITIALIZE_FUNC_NAME = "Py_Initialize";
static const char* PY_IS_TRUE_FUNC_NAME = "Py_IsTrue";
//static const char* PY_LIST_CHECK_FUNC_NAME = "PyList_Check";
static const char* PY_LIST_GET_ITEM_FUNC_NAME = "PyList_GetItem";
static const char* PY_LIST_SIZE_FUNC_NAME = "PyList_Size";
static const char* PY_LONG_AS_LONG_LONG_FUNC_NAME = "PyLong_AsLongLong";
static const char* PY_LONG_AS_UNSIGNED_LONG_LONG_FUNC_NAME = "PyLong_AsUnsignedLongLong";
static const char* PY_LONG_FROM_LONG_LONG_FUNC_NAME = "PyLong_FromLongLong";
static const char* PY_LONG_FROM_UNSIGNED_LONG_LONG_FUNC_NAME = "PyLong_FromUnsignedLongLong";
static const char* PY_MODULE_CREATE2_FUNC_NAME = "PyModule_Create2";
static const char* PY_MODULE_GET_DICT_FUNC_NAME = "PyModule_GetDict";
static const char* PY_MODULE_GET_STATE_FUNC_NAME = "PyModule_GetState";
static const char* PY_NEW_INTERPRETER_FUNC_NAME = "Py_NewInterpreter";
static const char* PY_OBJECT_CALL_FUNCTION_FUNC_NAME = "PyObject_CallFunction";
static const char* PY_OBJECT_GET_ATTR_STRING_FUNC_NAME = "PyObject_GetAttrString";
static const char* PY_RUN_ANY_FILE_FUNC_NAME = "PyRun_AnyFile";
static const char* PY_RUN_SIMPLE_STRING_FUNC_NAME = "PyRun_SimpleString";
static const char* PY_THREAD_STATE_GET_FUNC_NAME = "PyThreadState_Get";
//static const char* PY_TUPLE_CHECK_FUNC_NAME = "PyTuple_Check";
static const char* PY_TUPLE_GET_ITEM_FUNC_NAME = "PyTuple_GetItem";
static const char* PY_OBJECT_LENGTH_FUNC_NAME = "PyObject_Length";
static const char* PY_UNICODE_AS_UTF8_FUNC_NAME = "PyUnicode_AsUTF8";
static const char* PY_UNICODE_FROM_STRING_FUNC_NAME = "PyUnicode_FromString";

static const char* PY_EXC_RUNTIME_ERROR_DATA_NAME = "PyExc_RuntimeError";
static const char* PY_FALSE_STRUCT_DATA_NAME = "_Py_FalseStruct";
static const char* PY_NONE_STRUCT_DATA_NAME = "_Py_NoneStruct";
static const char* PY_TRUE_STRUCT_DATA_NAME = "_Py_TrueStruct";


void* (*PyBool_FromLong)(long) = nullptr;
void* (*Py_BuildValue)(const char*, ...) = nullptr;
int (*PyCallable_Check)(void*) = nullptr;
void* (*PyCapsule_GetPointer)(void*, const char*) = nullptr;
void* (*PyCapsule_New)(void*, const char*, void*) = nullptr;
void* (*PyCMethod_New)(void*, void*, void*) = nullptr;
void (*Py_DecRef)(void*) = nullptr;
void* (*PyDict_New)() = nullptr;
void* (*PyDict_Next)(void*, long long*, void**, void**) = nullptr;
int (*PyDict_SetItem)(void*, void*, void*) = nullptr;
int (*PyDict_SetItemString)(void*, const char*, void*) = nullptr;
void* (*PyErr_Occurred)() = nullptr;
void (*PyErr_PrintEx)(int) = nullptr;
void (*PyErr_SetString)(void*, const char*) = nullptr;
void (*PyEval_ReleaseThread)(void*) = nullptr;
void (*PyEval_RestoreThread)(void*) = nullptr;
double (*PyFloat_AsDouble)(void*) = nullptr;
int (*PyImport_AppendInitTab)(const char*, APPEND_INIT_TAB_FUNC_TYPE) = nullptr;
void* (*PyImport_ImportModule)(const char*) = nullptr;
void (*Py_IncRef)(void*) = nullptr;
void (*Py_Initialize)() = nullptr;
int (*Py_IsTrue)(void*) = nullptr;
//int (*PyList_Check)(void*) = nullptr;
void* (*PyList_GetItem)(void*, long long) = nullptr;
long long (*PyList_Size)(void*) = nullptr;
long long (*PyLong_AsLongLong)(void*) = nullptr;
unsigned long long (*PyLong_AsUnsignedLongLong)(void*) = nullptr;
void* (*PyLong_FromLongLong)(long long) = nullptr;
void* (*PyLong_FromUnsignedLongLong)(unsigned long long) = nullptr;
void* (*PyModule_Create2)(void*) = nullptr;
void* (*PyModule_GetDict)(void*) = nullptr;
void* (*PyModule_GetState)(void*) = nullptr;
void* (*Py_NewInterpreter)() = nullptr;
void* (*PyObject_CallFunction)(void*, const char*, ...) = nullptr;
void* (*PyObject_GetAttrString)(void*, const char*) = nullptr;
int (*PyRun_AnyFile)(FILE*, const char*) = nullptr;
int (*PyRun_SimpleString)(const char*) = nullptr;
void* (*PyThreadState_Get)() = nullptr;
//int (*PyTuple_Check)(void*) = nullptr;
void* (*PyTuple_GetItem)(void*, long long) = nullptr;
long long (*PyObject_Length)(void*) = nullptr;
const char* (*PyUnicode_AsUTF8)(void*) = nullptr;
void* (*PyUnicode_FromString)(const char*) = nullptr;

void* PY_EXC_RUNTIME_ERROR_DATA = nullptr;
void* PY_FALSE_STRUCT_DATA = nullptr;
void* PY_NONE_STRUCT_DATA = nullptr;
void* PY_TRUE_STRUCT_DATA = nullptr;

static bool py_dll_initialized = false;
struct AbstractNameToFunctionClass
{
  virtual ~AbstractNameToFunctionClass() = 0;
};

template<class T>
class NameToFunctionClass : public AbstractNameToFunctionClass
{
public:
  NameToFunctionClass(const char* new_name, T new_ptr)
  {
    function_name = new_name;
    func_ptr = new_ptr;
  }

  const char* function_name;
  T func_ptr;
};


void SetErrorCodeAndMessage(void* script_context_ptr, ScriptReturnCodes error_code, const char* error_msg)
{
  dolphinDefinedScriptContext_APIs.set_script_return_code(script_context_ptr, error_code);
  dolphinDefinedScriptContext_APIs.set_error_message(script_context_ptr, error_msg);
  delete python_lib_ptr;
  python_lib_ptr = nullptr;
  return;
}

template <class T>
bool GetFuncFromDLL(void* base_script_context_ptr, const char* func_name, T** func_ptr)
{
  *func_ptr = reinterpret_cast<T*>(python_lib_ptr->GetSymbolAddress(func_name));
  if (*func_ptr == nullptr)
  {
    SetErrorCodeAndMessage(base_script_context_ptr, ScriptReturnCodes::DLLComponentNotFoundError, (std::string("Error: ") + func_name + " component was not found in " + path_to_lib + " file.").c_str());
    return false;
  }
  return true;
}

bool AddNameAndFuncsToList(void* base_script_context_ptr)
{
  // Attempts to add each symbol's address to the specified location one-by-one, and returns immediately if
  // an attempt to set a symbol address pointer fails (since this uses short-circuit evaluation).
  return GetFuncFromDLL(base_script_context_ptr, PY_BOOL_FROM_LONG_FUNC_NAME, &PyBool_FromLong) &&
  GetFuncFromDLL(base_script_context_ptr, PY_BUILD_VALUE_FUNC_NAME, &Py_BuildValue) &&
  GetFuncFromDLL(base_script_context_ptr, PY_CALLABLE_CHECK_FUNC_NAME, &PyCallable_Check) &&
  GetFuncFromDLL(base_script_context_ptr, PY_CAPSULE_GET_POINTER_FUNC_NAME, &PyCapsule_GetPointer) &&
  GetFuncFromDLL(base_script_context_ptr, PY_CAPSULE_NEW_FUNC_NAME, &PyCapsule_New) &&
  GetFuncFromDLL(base_script_context_ptr, PY_CMETHOD_NEW, &PyCMethod_New) &&
  GetFuncFromDLL(base_script_context_ptr, PY_DEC_REF_FUNC_NAME, &Py_DecRef) &&
  GetFuncFromDLL(base_script_context_ptr, PY_DICT_NEW_FUNC_NAME, &PyDict_New) &&
  GetFuncFromDLL(base_script_context_ptr, PY_DICT_NEXT_FUNC_NAME, &PyDict_Next) &&
  GetFuncFromDLL(base_script_context_ptr, PY_DICT_SET_ITEM_FUNC_NAME, &PyDict_SetItem) &&
  GetFuncFromDLL(base_script_context_ptr, PY_DICT_SET_ITEM_STRING_FUNC_NAME, &PyDict_SetItemString) &&
  GetFuncFromDLL(base_script_context_ptr, PY_ERR_OCCURRED_FUNC_NAME, &PyErr_Occurred) &&
  GetFuncFromDLL(base_script_context_ptr, PY_ERR_PRINT_EX_FUNC_NAME, &PyErr_PrintEx) &&
  GetFuncFromDLL(base_script_context_ptr, PY_ERR_SET_STRING_FUNC_NAME, &PyErr_SetString) &&
  GetFuncFromDLL(base_script_context_ptr, PY_EVAL_RELEASE_THREAD_FUNC_NAME, &PyEval_ReleaseThread) &&
  GetFuncFromDLL(base_script_context_ptr, PY_EVAL_RESTORE_THREAD_FUNC_NAME, &PyEval_RestoreThread) &&
  GetFuncFromDLL(base_script_context_ptr, PY_FLOAT_AS_DOUBLE_FUNC_NAME, &PyFloat_AsDouble) &&
  GetFuncFromDLL(base_script_context_ptr, PY_IMPORT_APPEND_INIT_TAB_FUNC_NAME, &PyImport_AppendInitTab) &&
  GetFuncFromDLL(base_script_context_ptr, PY_IMPORT_IMPORT_MODULE_FUNC_NAME, &PyImport_ImportModule) &&
  GetFuncFromDLL(base_script_context_ptr, PY_INC_REF_FUNC_NAME, &Py_IncRef) &&
  GetFuncFromDLL(base_script_context_ptr, PY_INITIALIZE_FUNC_NAME, &Py_Initialize) &&
  GetFuncFromDLL(base_script_context_ptr, PY_IS_TRUE_FUNC_NAME, &Py_IsTrue) &&
  //GetFuncFromDLL(base_script_context_ptr, PY_LIST_CHECK_FUNC_NAME, &PyList_Check) &&
  GetFuncFromDLL(base_script_context_ptr, PY_LIST_GET_ITEM_FUNC_NAME, &PyList_GetItem) &&
  GetFuncFromDLL(base_script_context_ptr, PY_LIST_SIZE_FUNC_NAME, &PyList_Size) &&
  GetFuncFromDLL(base_script_context_ptr, PY_LONG_AS_LONG_LONG_FUNC_NAME, &PyLong_AsLongLong) &&
  GetFuncFromDLL(base_script_context_ptr, PY_LONG_AS_UNSIGNED_LONG_LONG_FUNC_NAME, &PyLong_AsUnsignedLongLong) &&
  GetFuncFromDLL(base_script_context_ptr, PY_LONG_FROM_LONG_LONG_FUNC_NAME, &PyLong_FromLongLong) &&
  GetFuncFromDLL(base_script_context_ptr, PY_LONG_FROM_UNSIGNED_LONG_LONG_FUNC_NAME, &PyLong_FromUnsignedLongLong) &&
  GetFuncFromDLL(base_script_context_ptr, PY_MODULE_CREATE2_FUNC_NAME, &PyModule_Create2) &&
  GetFuncFromDLL(base_script_context_ptr, PY_MODULE_GET_DICT_FUNC_NAME, &PyModule_GetDict) &&
  GetFuncFromDLL(base_script_context_ptr, PY_MODULE_GET_STATE_FUNC_NAME, &PyModule_GetState) &&
  GetFuncFromDLL(base_script_context_ptr, PY_NEW_INTERPRETER_FUNC_NAME, &Py_NewInterpreter) &&
  GetFuncFromDLL(base_script_context_ptr, PY_OBJECT_CALL_FUNCTION_FUNC_NAME, &PyObject_CallFunction) &&
  GetFuncFromDLL(base_script_context_ptr, PY_OBJECT_GET_ATTR_STRING_FUNC_NAME, &PyObject_GetAttrString) &&
  GetFuncFromDLL(base_script_context_ptr, PY_RUN_ANY_FILE_FUNC_NAME, &PyRun_AnyFile) &&
  GetFuncFromDLL(base_script_context_ptr, PY_RUN_SIMPLE_STRING_FUNC_NAME, &PyRun_SimpleString) &&
  GetFuncFromDLL(base_script_context_ptr, PY_THREAD_STATE_GET_FUNC_NAME, &PyThreadState_Get) &&
  //GetFuncFromDLL(base_script_context_ptr, PY_TUPLE_CHECK_FUNC_NAME, &PyTuple_Check) &&
  GetFuncFromDLL(base_script_context_ptr, PY_TUPLE_GET_ITEM_FUNC_NAME, &PyTuple_GetItem) &&
  GetFuncFromDLL(base_script_context_ptr, PY_OBJECT_LENGTH_FUNC_NAME, &PyObject_Length) &&
  GetFuncFromDLL(base_script_context_ptr, PY_UNICODE_AS_UTF8_FUNC_NAME, &PyUnicode_AsUTF8) &&
  GetFuncFromDLL(base_script_context_ptr, PY_UNICODE_FROM_STRING_FUNC_NAME, &PyUnicode_FromString) &&
  GetFuncFromDLL(base_script_context_ptr, PY_EXC_RUNTIME_ERROR_DATA_NAME, &PY_EXC_RUNTIME_ERROR_DATA) &&
  GetFuncFromDLL(base_script_context_ptr, PY_FALSE_STRUCT_DATA_NAME, &PY_FALSE_STRUCT_DATA) &&
  GetFuncFromDLL(base_script_context_ptr, PY_NONE_STRUCT_DATA_NAME, &PY_NONE_STRUCT_DATA) &&
  GetFuncFromDLL(base_script_context_ptr, PY_TRUE_STRUCT_DATA_NAME, &PY_TRUE_STRUCT_DATA);
}


void InitPythonLib(void* base_script_ptr)
{
  path_to_lib = std::string("C:/Users/skyle/OneDrive/Desktop/PythonStuff/python311.dll"); // TODO: Actually get the file from path
  python_lib_ptr = new DynamicLibrary(path_to_lib.c_str());

  if (!python_lib_ptr->IsOpen())
  {
    SetErrorCodeAndMessage(base_script_ptr, ScriptReturnCodes::DLLFileNotFoundError, "Error: DLL file for Python could not be located!");
    return;
  }

  if (!py_dll_initialized)
  {
    if (!AddNameAndFuncsToList(base_script_ptr))
      return;
  }
  py_dll_initialized = true;
}

void DeletePythonLib() {}
