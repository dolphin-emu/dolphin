#ifndef PYTHON_DYNAMIC_LIBRARY
#define PYTHON_DYNAMIC_LIBRARY

#include "DynamicLibrary.h"

namespace PythonDynamicLibrary
{
  extern DynamicLibrary* python_lib_ptr;

  void InitPythonLib(void* base_script_ptr);
  void DeletePythonLib();

  typedef void* (*APPEND_INIT_TAB_FUNC_TYPE)();

  extern void* (*PyBool_FromLong)(long);
  extern void* (*Py_BuildValue)(const char*, ...);
  extern int (*PyCallable_Check)(void*);
  extern void* (*PyCapsule_GetPointer)(void*, const char*);
  extern void* (*PyCapsule_New)(void*, const char*, void*);
  extern void* (*PyCMethod_New)(void*, void*, void*, void*);
  extern void (*Py_DecRef)(void*);
  extern void* (*PyDict_New)();
  extern void* (*PyDict_Next)(void*, long long*, void**, void**);
  extern int (*PyDict_SetItem)(void*, void*, void*);
  extern int (*PyDict_SetItemString)(void*, const char*, void*);
  extern void* (*PyErr_Occurred)();
  extern void (*PyErr_PrintEx)(int);
  extern void (*PyErr_SetString)(void*, const char*);
  extern void (*PyEval_ReleaseThread)(void*);
  extern void (*PyEval_RestoreThread)(void*);
  extern double (*PyFloat_AsDouble)(void*);
  extern int (*PyImport_AppendInittab)(const char*, APPEND_INIT_TAB_FUNC_TYPE);
  extern void* (*PyImport_ImportModule)(const char*);
  extern void (*Py_IncRef)(void*);
  extern void (*Py_Initialize)();
  extern int (*Py_IsTrue)(void*);
  //extern int (*PyList_Check)(void*);
  extern void* (*PyList_GetItem)(void*, long long);
  extern long long (*PyList_Size)(void*);
  extern long long (*PyLong_AsLongLong)(void*);
  extern unsigned long long (*PyLong_AsUnsignedLongLong)(void*);
  extern void* (*PyLong_FromLongLong)(long long);
  extern void* (*PyLong_FromUnsignedLongLong)(unsigned long long);
  extern void* (*PyModule_Create2)(void*, int);
  extern void* (*PyModule_GetDict)(void*);
  extern void* (*PyModule_GetState)(void*);
  extern void* (*Py_NewInterpreter)();
  extern void* (*PyObject_CallFunction)(void*, const char*, ...);
  extern void* (*PyObject_GetAttrString)(void*, const char*);
  extern int (*PyRun_AnyFile)(FILE*, const char*);
  extern int (*PyRun_SimpleString)(const char*);
  extern void* (*PyThreadState_Get)();
  //extern int (*PyTuple_Check)(void*);
  extern void* (*PyTuple_GetItem)(void*, long long);
  extern long long (*PyObject_Length)(void*);
  extern const char* (*PyUnicode_AsUTF8)(void*);
  extern void* (*PyUnicode_FromString)(const char*);

  extern void* PY_EXC_RUNTIME_ERROR_DATA;
  extern void* PY_FALSE_STRUCT_DATA;
  extern void* PY_NONE_STRUCT_DATA;
  extern void* PY_TRUE_STRUCT_DATA;
}
#endif
