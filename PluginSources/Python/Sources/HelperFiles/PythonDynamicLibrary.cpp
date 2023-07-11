#include "PythonDynamicLibrary.h"
#include "../DolphinDefinedAPIs.h"
#include "../CopiedScriptContextFiles/Enums/ScriptReturnCodes.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace PythonDynamicLibrary
{
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
  void* (*PyCMethod_New)(void*, void*, void*, void*) = nullptr;
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
  int (*PyImport_AppendInittab)(const char*, APPEND_INIT_TAB_FUNC_TYPE) = nullptr;
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
  void* (*PyModule_Create2)(void*, int) = nullptr;
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

#if defined(_WIN32)
  static const char PYTHON_PATH_SEPERATOR_CHAR = ';';
#else
  static const char PYTHON_PATH_SEPERATOR_CHAR = ":";
#endif

  void SetErrorCodeAndMessage(void* script_context_ptr, ScriptingEnums::ScriptReturnCodes error_code, const char* error_msg)
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
      SetErrorCodeAndMessage(base_script_context_ptr, ScriptingEnums::ScriptReturnCodes::DLLComponentNotFoundError, (std::string("Error: ") + func_name + " component was not found in " + path_to_lib + " file.").c_str());
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
      GetFuncFromDLL(base_script_context_ptr, PY_IMPORT_APPEND_INIT_TAB_FUNC_NAME, &PyImport_AppendInittab) &&
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


  bool stringIsBlank(std::string input)
  {
    unsigned long long str_length = input.length();
    for (unsigned long long i = 0; i < str_length; ++i)
    {
      if (!isspace(input[i]))
        return false;
    }
    return true;
  }
  std::vector<std::string> getPathsFromEnvironmentValue(std::string environment_variable)
  {
    std::vector<std::string> return_vector = std::vector<std::string>();
    std::string next_path = "";
    unsigned long long env_var_length = environment_variable.size();
    for (unsigned long long i = 0; i < env_var_length; ++i)
    {
      if (environment_variable[i] == PYTHON_PATH_SEPERATOR_CHAR) // we split on colons in the path string for Linux/Unix/Mac, and semi-colons for Windows.
      {
        if (next_path.length() > 0 && !stringIsBlank(next_path))
          return_vector.push_back(next_path);
        next_path = "";
      }
      else if (environment_variable[i] == '"' || environment_variable[i] == '\'')
        continue;
      else
        next_path = next_path + environment_variable[i];
    }
    if (next_path.length() > 0 && !stringIsBlank(next_path))
      return_vector.push_back(next_path);

    return return_vector;
  }

#if defined(_WIN32)
  static const std::string LIBRARY_SUFFIX = std::string(".dll");
#elif defined(__APPLE__)
  static const std::string LIBRARY_SUFFIX = std::string(".dylib");
#else
  static const std::string LIBRARY_SUFFIX = std::string(".so");
#endif

  // python311_d.dll
  bool stringIsPathToPythonLib(std::string path_string)
  {
    unsigned long long path_string_length = path_string.length();
    if (path_string.empty() || path_string_length < LIBRARY_SUFFIX.length() + 1)
      return false;

    if (
      (path_string[0] == 'p' || path_string[0] == 'P') &&
      (path_string[1] == 'y' || path_string[1] == 'Y') &&
      (path_string[2] == 't' || path_string[2] == 'T') &&
      (path_string[3] == 'h' || path_string[3] == 'H') &&
      (path_string[4] == 'o' || path_string[4] == 'O') &&
      (path_string[5] == 'n' || path_string[5] == 'N')
      )
    {
      int number_of_digits = 0;
      int index_in_string = 6;
      while (index_in_string < path_string_length && isdigit(path_string[index_in_string]))
      {
        ++number_of_digits;
        ++index_in_string;
      }
      if (index_in_string >= path_string_length)
        return false;

      if (number_of_digits < 2) // NOTE: This code will work until Python10 comes out. However, based on Python's release schedule, that probably won't happen for another 150 years - so this should be safe for the forseeable future...
        return false;

#ifndef NDEBUG // case where we're in debug mode
      if (path_string[index_in_string] == '_' &&
        (path_string[index_in_string + 1] == 'd') || path_string[index_in_string + 1] == 'D')
        index_in_string += 2;
      else
        return false;
#endif

      if (index_in_string < path_string_length && path_string.substr(index_in_string) == LIBRARY_SUFFIX)
        return true;
    }
    return false;
  }

  void GetPythonLibFromEnvVariable(const char* env_variable)
  {
    const char* raw_env_value = getenv(env_variable);
    if (raw_env_value == nullptr)
      return;

    std::vector<std::string> python_paths_to_search = getPathsFromEnvironmentValue(raw_env_value);
    unsigned long long number_of_paths = python_paths_to_search.size();

    for (unsigned long long i = 0; i < number_of_paths; ++i)
    {
      std::string path_to_search = python_paths_to_search[i];
      if (std::filesystem::is_directory(std::filesystem::path(path_to_search)))
      {
        for (auto& entry : std::filesystem::directory_iterator(path_to_search))
        {
          const std::string inner_filename = entry.path().filename().string();
          if (!entry.is_directory() && stringIsPathToPythonLib(inner_filename))
          {
            path_to_lib = inner_filename;
            return;
          }
        }
      }
      else if (stringIsPathToPythonLib(path_to_search))
      {
        path_to_lib = path_to_search;
        return;
      }
    }
  }

  void InitPythonLib(void* base_script_ptr)
  {
    if (python_lib_ptr != nullptr)
      return;

    path_to_lib = "";
    GetPythonLibFromEnvVariable("DOLPHIN_PYTHON_PATH");
    if (path_to_lib.empty())
      GetPythonLibFromEnvVariable("PYTHONPATH");
    if (path_to_lib.empty())
      GetPythonLibFromEnvVariable("PYTHONHOME");
    if (path_to_lib.empty())
      GetPythonLibFromEnvVariable("PATH");

    if (path_to_lib.empty())
    {
      SetErrorCodeAndMessage(base_script_ptr, ScriptingEnums::ScriptReturnCodes::DLLFileNotFoundError, (std::string("Error: The ") + LIBRARY_SUFFIX + " file for Python could not be located. Environment variables that were searched for Python DLLs (in order) were DOLPHIN_PYTHON_PATH, PYTHONPATH, PYTHONHOME and PATH").c_str());
      return;
    }

    python_lib_ptr = new DynamicLibrary(path_to_lib.c_str());

    if (!python_lib_ptr->IsOpen())
    {
      SetErrorCodeAndMessage(base_script_ptr, ScriptingEnums::ScriptReturnCodes::DLLFileNotFoundError, (std::string("Error: The ") + path_to_lib + " file for Python could not be opened!").c_str());
      DeletePythonLib();
      return;
    }

    // Indicates that an error occured, and a function couldn't be found.
    if (!AddNameAndFuncsToList(base_script_ptr))
      DeletePythonLib();
  }

  void DeletePythonLib()
  {
    if (python_lib_ptr == nullptr)
      return;

    delete python_lib_ptr;
    python_lib_ptr = nullptr;
  }
}
