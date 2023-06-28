#include "PythonDynamicLibrary.h"

DynamicLibrary* python_lib_ptr = nullptr;

bool InitPythonLib() { return true; }
void DeletePythonLib() {}

const char* CAPSULE_GET_POINTER_NAME = "PyCapsule_GetPointer";
const char* IMPORT_MODULE_NAME = "PyImport_ImportModule";
const char* MODULE_GET_STATE_NAME = "PyModule_GetState";
const char* INC_REF_NAME = "Py_IncRef";
const char* DEC_REF_NAME = "Py_DecRef";
const char* PY_NONE_STRUCT_NAME = "_Py_NoneStruct";
const char* PY_TRUE_STRUCT_NAME = "_Py_TrueStruct";
const char* PY_FALSE_STRUCT_NAME = "_Py_FalseStruct";
const char* PYTHON_BUILD_VALUE_NAME = "Py_BuildValue";
const char* PYTHON_RUN_STRING_NAME = "PyRun_SimpleString";
const char* PYTHON_RUN_FILE_NAME = "PyRun_AnyFile";
const char* PYTHON_SET_ERR_NAME = "PyErr_SetString";
const char* PYTHON_ERROR_OCCURED_NAME = "PyErr_Occured";
const char* PY_ERR_PRINT_EX_NAME = "PyErr_PrintEx";
const char* PY_OBJECT_GET_ATTR_STRING_NAME = "PyObject_GetAttrString";
const char* PY_UNICODE_AS_UTF8_NAME = "PyUnicode_AsUTF8";
const char* PY_OBJECT_CALL_FUNCTION_NAME = "PyObject_CallFunction";
const char* PY_INITIALIZE_NAME = "Py_Initialize";
const char* PY_THREAD_STATE_GET_NAME = "PyThreadState_Get";
const char* PY_EVAL_RESTORE_THREAD_NAME = "PyEval_RestoreThread";
const char* PY_EVAL_RELEASE_THREAD_NAME = "PyEval_ReleaseThread";
const char* PY_NEW_INTERPRETER_NAME = "Py_NewInterpreter";
const char* PY_MODULE_CREATE_NAME = "PyModule_Create";
const char* APPEND_INIT_TAB_SYMBOL_NAME = "PyImport_AppendInitTab";
const char* PY_UNICODE_AS_STRING_NAME = "PyUnicode_AsString";
const char* PY_MODULE_GET_DICT_NAME = "PyModule_GetDict";
const char* PY_CAPSULE_NEW_NAME = "PyCapsule_New";
const char* PY_CFUNCTION_NEW_EX = "PyCFunction_NewEx";
const char* PY_DICT_SET_STRING_NAME = "PyDict_SetItemString";
const char* PY_CALLABLE_CHECK_NAME = "PyCallable_Check";
const char* PY_IS_TRUE_NAME = "Py_IsTrue";
const char* PY_TUPLE_GET_SIZE_NAME = "PyTuple_GET_SIZE";
const char* PY_TUPLE_GET_ITEM_NAME = "PyTuple_GetItem";
const char* PY_TUPLE_CHECK_NAME = "PyTuple_Check";
const char* PY_LONG_AS_UNSIGNED_LONG_LONG_NAME = "PyLong_AsUnsignedLongLong";
const char* PY_LONG_AS_LONG_LONG_NAME = "PyLong_AsLongLong";
const char* PY_LONG_FROM_LONG_LONG_NAME = "PyLong_FromLongLong";
const char* PY_LONG_FROM_UNSIGNED_LONG_LONG_NAME = "PyLong_FromUnsignedLongLong";
const char* PY_FLOAT_AS_DOUBLE_NAME = "PyFloat_AsDouble";
const char* PY_DICT_NEW_NAME = "PyDict_New";
const char* PY_DICT_SET_ITEM_NAME = "PyDict_SetItem";
const char* PY_DICT_NEXT_NAME = "PyDict_Next";
const char* PY_LIST_SIZE_NAME = "PyList_Size";
const char* PY_LIST_GET_ITEM_NAME = "PyList_GetItem";
const char* PY_LIST_CHECK_NAME = "PyList_Check";
const char* PY_UNICODE_FROM_STRING_NAME = "PyUnicode_FromString";
const char* PY_BOOL_FROM_LONG_NAME = "PyBool_FromLong";
