#include "PythonInterface.h"
#include "../PythonScriptContextFiles/PythonScriptContext.h"
#include <string>
#include <vector>
#include "PythonDynamicLibrary.h"

namespace PythonInterface
{
  const char* function_metadata_capsule_name = "functionMetadataCapsule";
  const char* THIS_MODULE_NAME = "ThisBaseScriptModuleNamesdbkhcjs";
  static const char* redirect_output_module_name = "RedirectStdOut";

  typedef int (*visitproc)(void*, void*);
  typedef int (*traverseproc)(void*, visitproc, void*);
  typedef int (*inquiry)(void*);
  typedef void (*freefunc)(void*);

  typedef struct Cust_PyObject
  {
    long long ob_refcnt;
    void* ob_type;
  } Custom_PyObject;

  typedef struct Cust_PyModuleDefBase
  {
    Custom_PyObject ob_base;
    void* (*m_init)();
    long long m_index;
    void* m_copy;

  } Custom_PyModuleDefBase;

  typedef struct Cust_PyModuleDef
  {
    Custom_PyModuleDefBase m_base;
    const char* m_name;
    const char* m_doc;
    long long m_size;
    void* m_methods;
    void* m_slots;
    traverseproc m_traverse;
    inquiry m_clear;
    freefunc m_free;
  } Custom_PyModuleDef;


  typedef struct Cust_PyMethodDef {
    const char* ml_name;
    void* (*ml_meth)(void*, void*);
    int         ml_flags;
    const char* ml_doc;

  } Custom_PyMethodDef;

  static void* RunFunction(void* self, void* args)
  {
    FunctionMetadata* current_function_metadata = reinterpret_cast<FunctionMetadata*>(PythonDynamicLibrary::PyCapsule_GetPointer(self, function_metadata_capsule_name));
    void* base_script_context_ptr = reinterpret_cast<void*>(*((unsigned long long*)PythonDynamicLibrary::PyModule_GetState(PythonDynamicLibrary::PyImport_ImportModule(THIS_MODULE_NAME))));

   return RunFunction_impl(base_script_context_ptr, current_function_metadata, self, args);
  }

  void Python_IncRef(void* raw_py_obj)
  {
    PythonDynamicLibrary::Py_IncRef(raw_py_obj);
  }

  void Python_DecRef(void* raw_py_obj)
  {
    PythonDynamicLibrary::Py_DecRef(raw_py_obj);
  }

  void* GetNoneObject() {
    void* py_none = PythonDynamicLibrary::PY_NONE_STRUCT_DATA;
    PythonDynamicLibrary::Py_IncRef(py_none);
    return py_none;
  }

  void* GetPyTrueObject()
  {
    void* py_true = PythonDynamicLibrary::PY_TRUE_STRUCT_DATA;
    PythonDynamicLibrary::Py_IncRef(py_true);
    return py_true;
  }

  void* GetPyFalseObject()
  {
    void* py_false = PythonDynamicLibrary::PY_FALSE_STRUCT_DATA;
    PythonDynamicLibrary::Py_IncRef(py_false);
    return py_false;
  }

  void* Python_BuildValue(const char* format_string, void* ptr_to_val)
  {
    switch (format_string[0])
    {
    case 'K':
      return PythonDynamicLibrary::Py_BuildValue("K", *(reinterpret_cast<unsigned long long*>(ptr_to_val)));
    case 'L':
      return PythonDynamicLibrary::Py_BuildValue("L", *(reinterpret_cast<signed long long*>(ptr_to_val)));
    case 'f':
      return PythonDynamicLibrary::Py_BuildValue("f", *(reinterpret_cast<float*>(ptr_to_val)));
    case 'd':
      return PythonDynamicLibrary::Py_BuildValue("d", *(reinterpret_cast<double*>(ptr_to_val)));
    case 's':
      return PythonDynamicLibrary::Py_BuildValue("s", *(reinterpret_cast<const char**>(ptr_to_val)));
    default:
      return nullptr;
    }
  }

  void Python_RunString(const char* string_to_run)
  {
    PythonDynamicLibrary::PyRun_SimpleString(string_to_run);
  }

  void Python_RunFile(const char* file_name)
  {
    FILE* fp = fopen(file_name, "rb");
    PythonDynamicLibrary::PyRun_AnyFile(fp, nullptr);
    fclose(fp);
  }

  void Python_SetRunTimeError(const char* error_msg)
  {
    PythonDynamicLibrary::PyErr_SetString(PythonDynamicLibrary::PY_EXC_RUNTIME_ERROR_DATA, error_msg);
  }

  bool Python_ErrOccured()
  {
    return PythonDynamicLibrary::PyErr_Occurred();
  }

  void Python_CallPyErrPrintEx()
  {
    PythonDynamicLibrary::PyErr_PrintEx(1);
  }

  void Python_SendOutputToCallbackLocationAndClear(void* base_script_context_ptr, void (*print_callback)(void*, const char*))
  {
    void* pModule = PythonDynamicLibrary::PyImport_ImportModule(redirect_output_module_name);
    void* catcher = PythonDynamicLibrary::PyObject_GetAttrString(pModule, "catchOutErr");  // get our catchOutErr created above
    void* output = PythonDynamicLibrary::PyObject_GetAttrString(catcher, "value");

    const char* output_msg = PythonDynamicLibrary::PyUnicode_AsUTF8(output);
    if (output_msg != nullptr && !std::string(output_msg).empty())
      print_callback(base_script_context_ptr, output_msg);

    void* clear_method = PythonDynamicLibrary::PyObject_GetAttrString(catcher, "clear");
    PythonDynamicLibrary::PyObject_CallFunction(clear_method, nullptr);
  }

  void Python_Initialize()
  {
    PythonDynamicLibrary::Py_Initialize();
  }

  void* PythonThreadState_Get()
  {
    return PythonDynamicLibrary::PyThreadState_Get();
  }

  void PythonEval_RestoreThread(void* python_thread)
  {
    PythonDynamicLibrary::PyEval_RestoreThread(python_thread);
  }

  void PythonEval_ReleaseThread(void* python_thread)
  {
    PythonDynamicLibrary::PyEval_ReleaseThread(python_thread);
  }

  void* Python_NewInterpreter()
  {
    return PythonDynamicLibrary::Py_NewInterpreter();
  }


  Custom_PyModuleDef ThisModuleDef = {
    { { 1, NULL }, NULL, NULL, NULL },
    THIS_MODULE_NAME,
    nullptr,
    sizeof(unsigned long long),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
  };

  extern "C" __declspec(dllexport) void* internal_this_mod_create_func()
  {
    return PythonDynamicLibrary::PyModule_Create2(&ThisModuleDef, 1013);
  }

  void CreateThisModule()
  {
    PythonDynamicLibrary::PyImport_AppendInittab(THIS_MODULE_NAME, internal_this_mod_create_func);
  }

  bool SetThisModule(void* base_script_context_ptr)
  {
    void* this_module = PythonDynamicLibrary::PyImport_ImportModule(THIS_MODULE_NAME);
    if (this_module == nullptr)
      return false;
    void* this_module_state = PythonDynamicLibrary::PyModule_GetState(this_module);
    if (this_module_state == nullptr)
      return false;

    *(reinterpret_cast<unsigned long long*>(this_module_state)) = reinterpret_cast<unsigned long long>(base_script_context_ptr);
    return true;
  }

  Custom_PyModuleDef generic_mod_def = {
    { { 1, NULL }, NULL, NULL, NULL },
    "genericModuleName",
    "genericDocumentationString",
    sizeof(std::string),
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr
  };

  extern "C" __declspec(dllexport) void* internal_module_create_fnc()
  {
    return PythonDynamicLibrary::PyModule_Create2(&generic_mod_def, 1013);
  }

  void CreateEmptyModule(const char* new_module_name)
  {
    // Since this is only called internally, this condition should never be true, but whatever...
    if (new_module_name == nullptr || new_module_name[0] == '\0')
      return;

    PythonDynamicLibrary::PyImport_AppendInittab(new_module_name, internal_module_create_fnc);
  }

  bool SetModuleVersion(const char* module_name, const char* version_string)
  {
    if (module_name == nullptr || module_name[0] == '\0' || version_string == nullptr || version_string[0] == '\0')
      return false;

    void* imported_module = PythonDynamicLibrary::PyImport_ImportModule(module_name);
    if (imported_module == nullptr)
      return false;
    void* module_state = PythonDynamicLibrary::PyModule_GetState(imported_module);
    if (module_state == nullptr)
      return false;
    *((std::string*)module_state) = std::string(version_string);
    return true;
  }

  // Note that this class_metadata object is allocated on the heap.
  bool SetModuleFunctions(const char* module_name, ClassMetadata* class_metadata, std::vector<void*>* ptr_to_vector_of_py_method_defs_to_delete)
  {
    if (module_name == nullptr || module_name[0] == '\0' || class_metadata == nullptr)
      return false;
    void* module_obj = PythonDynamicLibrary::PyImport_ImportModule(module_name);
    if (module_obj == nullptr)
      return false;
    void* module_name_as_obj = PythonDynamicLibrary::PyUnicode_FromString(module_name);
    void* module_dict = PythonDynamicLibrary::PyModule_GetDict(module_obj);
    if (module_name_as_obj == nullptr || module_dict == nullptr)
      return false;

    unsigned long long num_methods = class_metadata->functions_list.size();

    for (unsigned long long i = 0; i < num_methods; ++i)
    {
      Custom_PyMethodDef* new_py_method_def = new Custom_PyMethodDef();
      ptr_to_vector_of_py_method_defs_to_delete->push_back(reinterpret_cast<void*>(new_py_method_def));
      new_py_method_def->ml_name = class_metadata->functions_list[i].function_name.c_str();
      new_py_method_def->ml_doc = class_metadata->functions_list[i].example_function_call.c_str();
      new_py_method_def->ml_flags = 0x0001;
      new_py_method_def->ml_meth = RunFunction;

      void* fnc_capsule_obj = PythonDynamicLibrary::PyCapsule_New(&(class_metadata->functions_list[i]), function_metadata_capsule_name, nullptr);
      void* new_fnc = PythonDynamicLibrary::PyCMethod_New(new_py_method_def, fnc_capsule_obj, module_name_as_obj, NULL);
      PythonDynamicLibrary::PyDict_SetItemString(module_dict, class_metadata->functions_list[i].function_name.c_str(), new_fnc);
    }
    return true;
  }

  void DeleteMethodDefsVector(std::vector<void*>* method_defs_to_delete)
  {
    unsigned long long size = method_defs_to_delete->size();

    for (unsigned long long i = 0; i < size; ++i)
    {
      Custom_PyMethodDef* current_py_method_def = reinterpret_cast<Custom_PyMethodDef*>(method_defs_to_delete->at(i));
      delete current_py_method_def;
    }

    method_defs_to_delete->clear();
  }

  const char* GetModuleVersion(const char* module_name)
  {
    return (*((std::string*)PythonDynamicLibrary::PyModule_GetState(PythonDynamicLibrary::PyImport_ImportModule(module_name)))).c_str();
  }

  bool RunImportCommand(const char* module_name)
  {
    PythonDynamicLibrary::PyRun_SimpleString((std::string("import ") + module_name).c_str());
    return true;
  }

  void AppendArgumentsToPath(const char* user_path, const char* system_path)
  {
    PythonDynamicLibrary::PyRun_SimpleString((std::string("import sys\nsys.path.append('") + user_path + "')\n").c_str());
    PythonDynamicLibrary::PyRun_SimpleString((std::string("sys.path.append('") + system_path + "')\n").c_str());
  }

  void RedirectStdOutAndStdErr()
  {
    PythonDynamicLibrary::PyRun_SimpleString((std::string("import ") + redirect_output_module_name + "\n")
      .c_str());  // invoke code to redirect
    PythonDynamicLibrary::PyImport_ImportModule(redirect_output_module_name);
  }

  bool Python_IsCallable(void* raw_py_obj)
  {
    return PythonDynamicLibrary::PyCallable_Check(raw_py_obj);
  }

  void* PythonObject_CallFunction(void* py_obj)
  {
    PythonDynamicLibrary::Py_IncRef(py_obj);
    void* return_value = PythonDynamicLibrary::PyObject_CallFunction(py_obj, nullptr);
    PythonDynamicLibrary::Py_DecRef(py_obj);
    return return_value;
  }

  bool PythonObject_IsTrue(void* py_obj_raw)
  {
    return PythonDynamicLibrary::Py_IsTrue(py_obj_raw);
  }

  unsigned long long PythonTuple_GetSize(void* raw_python_tuple)
  {
    return PythonDynamicLibrary::PyObject_Length(raw_python_tuple);
  }

  void* PythonTuple_GetItem(void* raw_python_tuple, unsigned long long index)
  {
    return PythonDynamicLibrary::PyTuple_GetItem(raw_python_tuple, index);
  }

  /*
  bool PythonTuple_Check(void* raw_python_arg)
  {
    return PyTuple_Check(castToPyObject(raw_python_arg));
  }*/

  unsigned long long PythonLongObj_AsU64(void* raw_python_obj)
  {
    return PythonDynamicLibrary::PyLong_AsUnsignedLongLong(raw_python_obj);
  }

  signed long long PythonLongObj_AsS64(void* raw_python_obj)
  {
    return PythonDynamicLibrary::PyLong_AsLongLong(raw_python_obj);
  }

  void* S64_ToPythonLongObj(signed long long input_s64)
  {
    return PythonDynamicLibrary::PyLong_FromLongLong(input_s64);
  }

  void* U64_ToPythonLongObj(unsigned long long input_u64)
  {
    return PythonDynamicLibrary::PyLong_FromUnsignedLongLong(input_u64);
  }

  double PythonFloatObj_AsDouble(void* raw_python_obj)
  {
    return PythonDynamicLibrary::PyFloat_AsDouble(raw_python_obj);
  }

  const char* PythonUnicodeObj_AsString(void* raw_python_obj)
  {
    return PythonDynamicLibrary::PyUnicode_AsUTF8(raw_python_obj);
  }

  void* PythonDictionary_New()
  {
    return PythonDynamicLibrary::PyDict_New();
  }

  void PythonDictionary_SetItem(void* raw_dict_obj, void* raw_key_obj, void* raw_val_obj)
  {
    PythonDynamicLibrary::PyDict_SetItem(raw_dict_obj, raw_key_obj, raw_val_obj);
  }

  void* key = nullptr;
  void* value = nullptr;

  void* ResetAndGetRef_ToPyKey()
  {
    key = nullptr;
    return key;
  }

  void* ResetAndGetRef_ToPyVal()
  {
    value = nullptr;
    return value;
  }

  // Since everything in Python has a global lock, that means that only one python function can be executing at a time.
  // Also, functions cannot be interrupted by another thread/script once they've started, which means it's safe
  // to have a static value for the key and value that get passed into this function (via the above 2 functions)
  bool PythonDict_Next(void* raw_dict_obj, signed long long* ptr_to_index, void** raw_key_ptr_ptr, void** raw_value_ptr_ptr)
  {
    return PythonDynamicLibrary::PyDict_Next(raw_dict_obj, ptr_to_index, raw_key_ptr_ptr, raw_value_ptr_ptr);
  }

  unsigned long long PythonList_Size(void* raw_py_obj)
  {
    return PythonDynamicLibrary::PyList_Size(raw_py_obj);
  }

  void* PythonList_GetItem(void* raw_py_list, unsigned long long index)
  {
    return PythonDynamicLibrary::PyList_GetItem(raw_py_list, index);
  }

  /*bool PythonList_Check(void* raw_py_obj)
  {
    return PyList_Check(castToPyObject(raw_py_obj));
  }*/

  void* StringTo_PythonUnicodeObj(const char* input_str)
  {
    return PythonDynamicLibrary::PyUnicode_FromString(input_str);
  }

  void* PythonBooleanObj_FromLong(long long input_long_long)
  {
    return PythonDynamicLibrary::PyBool_FromLong(input_long_long);
  }
}
