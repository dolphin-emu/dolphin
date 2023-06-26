#include "PythonInterface.h"
#include "Python.h"
#include "../PythonScriptContextFiles/PythonScriptContext.h"
#include <string>
#include <vector>


namespace PythonInterface
{
  const char* function_metadata_capsule_name = "functionMetadataCapsule";
  const char* THIS_MODULE_NAME = "ThisBaseScriptModuleNamesdbkhcjs";
  static const char* redirect_output_module_name = "RedirectStdOut";

  PyObject* castToPyObject(void* input)
  {
    return reinterpret_cast<PyObject*>(input);
  }

  void* castToVoidPtr(PyObject* input)
  {
    return reinterpret_cast<void*>(input);
  }

  static PyObject* RunFunction(PyObject* self, PyObject* args)
  {
    FunctionMetadata* current_function_metadata = reinterpret_cast<FunctionMetadata*>(PyCapsule_GetPointer(self, function_metadata_capsule_name));
    void* base_script_context_ptr = reinterpret_cast<void*>(*((unsigned long long*)PyModule_GetState(PyImport_ImportModule(THIS_MODULE_NAME))));

    PyObject* ret_val = castToPyObject(RunFunction_impl(base_script_context_ptr, current_function_metadata, castToVoidPtr(self), castToVoidPtr(args)));
    return ret_val;
  }

  void Python_IncRef(void* raw_py_obj)
  {
    Py_INCREF(castToPyObject(raw_py_obj));   
  }

  void Python_DecRef(void* raw_py_obj)
  {
    Py_DECREF(castToPyObject(raw_py_obj));
  }

  void* GetNoneObject() {
    Py_RETURN_NONE;
  }

  void* GetPyTrueObject()
  {
    Py_RETURN_TRUE;
  }

  void* GetPyFalseObject()
  {
    Py_RETURN_FALSE;
  }

  void* Python_BuildValue(const char* format_string, void* ptr_to_val)
  {
    switch (format_string[0])
    {
    case 'K':
      return castToVoidPtr(Py_BuildValue("K", *(reinterpret_cast<unsigned long long*>(ptr_to_val))));
    case 'L':
      return castToVoidPtr(Py_BuildValue("L", *(reinterpret_cast<signed long long*>(ptr_to_val))));
    case 'f':
      return castToVoidPtr(Py_BuildValue("f", *(reinterpret_cast<float*>(ptr_to_val))));
    case 'd':
      return castToVoidPtr(Py_BuildValue("d", *(reinterpret_cast<double*>(ptr_to_val))));
    case 's':
      return castToVoidPtr(Py_BuildValue("s", *(reinterpret_cast<const char**>(ptr_to_val))));
    default:
      return nullptr;
    }
  }

  void Python_RunString(const char* string_to_run)
  {
    PyRun_SimpleString(string_to_run);
  }

  void Python_RunFile(const char* file_name)
  {
    FILE* fp = fopen(file_name, "rb");
    PyRun_AnyFile(fp, nullptr);
    fclose(fp);
  }

  void Python_SetRunTimeError(const char* error_msg)
  {
    PyErr_SetString(PyExc_RuntimeError, error_msg);
  }

  bool Python_ErrOccured()
  {
    return PyErr_Occurred();
  }

  void Python_CallPyErrPrintEx()
  {
    PyErr_PrintEx(1);
  }

  void Python_SendOutputToCallbackLocationAndClear(void* base_script_context_ptr, void (*print_callback)(void*, const char*))
  {
    PyObject* pModule = PyImport_ImportModule(redirect_output_module_name);
    PyObject* catcher = PyObject_GetAttrString(pModule, "catchOutErr");  // get our catchOutErr created above
    PyObject* output = PyObject_GetAttrString(catcher, "value");

    const char* output_msg = PyUnicode_AsUTF8(output);
    if (output_msg != nullptr && !std::string(output_msg).empty())
      print_callback(base_script_context_ptr, output_msg);

    PyObject* clear_method = PyObject_GetAttrString(catcher, "clear");
    PyObject_CallFunction(clear_method, nullptr);
  }

  void Python_Initialize()
  {
    Py_Initialize();
  }

  void* PythonThreadState_Get()
  {
    return reinterpret_cast<void*>(PyThreadState_Get());
  }

  void PythonEval_RestoreThread(void* python_thread)
  {
    PyThreadState* python_cast_thread = reinterpret_cast<PyThreadState*>(python_thread);
    PyEval_RestoreThread(python_cast_thread);
  }

  void PythonEval_ReleaseThread(void* python_thread)
  {
    PyThreadState* python_cast_thread = reinterpret_cast<PyThreadState*>(python_thread);
    PyEval_ReleaseThread(python_cast_thread);
  }

  void* Python_NewInterpreter() { return reinterpret_cast<void*>(Py_NewInterpreter()); }

  PyModuleDef ThisModuleDef = {
    PyModuleDef_HEAD_INIT, THIS_MODULE_NAME,
    nullptr,
    sizeof(unsigned long long),
    nullptr };

  PyMODINIT_FUNC internal_this_mod_create_func()
  {
    return PyModule_Create(&ThisModuleDef);
  }

  void CreateThisModule()
  {
    PyImport_AppendInittab(THIS_MODULE_NAME, internal_this_mod_create_func);
  }

  bool SetThisModule(void* base_script_context_ptr)
  {
    PyObject* this_module = PyImport_ImportModule(THIS_MODULE_NAME);
    if (this_module == nullptr)
      return false;
    void* this_module_state = PyModule_GetState(this_module);
    if (this_module_state == nullptr)
      return false;

    *(reinterpret_cast<unsigned long long*>(this_module_state)) = reinterpret_cast<unsigned long long>(base_script_context_ptr);
    return true;
  }

  PyModuleDef generic_mod_def = {
    PyModuleDef_HEAD_INIT,
    "genericModuleName",
    "genericDocumentationString",
    sizeof(std::string),
    nullptr
  };

  PyMODINIT_FUNC internal_module_create_fnc()
  {
    return PyModule_Create(&generic_mod_def);
  }

  void CreateEmptyModule(const char* new_module_name)
  {
    // Since this is only called internally, this condition should never be true, but whatever...
    if (new_module_name == nullptr || new_module_name[0] == '\0')
      return;

    PyImport_AppendInittab(new_module_name, internal_module_create_fnc);
  }

  bool SetModuleVersion(const char* module_name, const char* version_string)
  {
    if (module_name == nullptr || module_name[0] == '\0' || version_string == nullptr || version_string[0] == '\0')
      return false;

    PyObject* imported_module = PyImport_ImportModule(module_name);
    if (imported_module == nullptr)
      return false;
    void* module_state = PyModule_GetState(imported_module);
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
    PyObject* module_obj = PyImport_ImportModule(module_name);
    if (module_obj == nullptr)
      return false;
    PyObject* module_name_as_obj = PyUnicode_FromString(module_name);
    PyObject* module_dict = PyModule_GetDict(module_obj);
    if (module_name_as_obj == nullptr || module_dict == nullptr)
      return false;

    unsigned long long num_methods = class_metadata->functions_list.size();

    for (unsigned long long i = 0; i < num_methods; ++i)
    {
      PyMethodDef* new_py_method_def = new PyMethodDef();
      ptr_to_vector_of_py_method_defs_to_delete->push_back(reinterpret_cast<void*>(new_py_method_def));
      new_py_method_def->ml_name = class_metadata->functions_list[i].function_name.c_str();
      new_py_method_def->ml_doc = class_metadata->functions_list[i].example_function_call.c_str();
      new_py_method_def->ml_flags = METH_VARARGS;
      new_py_method_def->ml_meth = RunFunction;

      PyObject* fnc_capsule_obj = PyCapsule_New(&(class_metadata->functions_list[i]), function_metadata_capsule_name, nullptr);
      PyObject* new_fnc = PyCFunction_NewEx(new_py_method_def, fnc_capsule_obj, module_name_as_obj);
      PyDict_SetItemString(module_dict, class_metadata->functions_list[i].function_name.c_str(), new_fnc);
    }
    return true;
  }

  void DeleteMethodDefsVector(std::vector<void*>* method_defs_to_delete)
  {
    unsigned long long size = method_defs_to_delete->size();

    for (unsigned long long i = 0; i < size; ++i)
    {
      PyMethodDef* current_py_method_def = reinterpret_cast<PyMethodDef*>(method_defs_to_delete->at(i));
      delete current_py_method_def;
    }

    method_defs_to_delete->clear();
  }

  const char* GetModuleVersion(const char* module_name)
  {
    return (*((std::string*)PyModule_GetState(PyImport_ImportModule(module_name)))).c_str();
  }

  bool RunImportCommand(const char* module_name)
  {
    PyRun_SimpleString((std::string("import ") + module_name).c_str());
    return true;
  }

  void AppendArgumentsToPath(const char* user_path, const char* system_path)
  {
    PyRun_SimpleString((std::string("import sys\nsys.path.append('") + user_path + "')\n").c_str());
    PyRun_SimpleString((std::string("sys.path.append('") + system_path + "')\n").c_str());
  }

  void RedirectStdOutAndStdErr()
  {
    PyRun_SimpleString((std::string("import ") + redirect_output_module_name + "\n")
      .c_str());  // invoke code to redirect
    PyImport_ImportModule(redirect_output_module_name);
  }

  bool Python_IsCallable(void* raw_py_obj)
  {
    return PyCallable_Check(castToPyObject(raw_py_obj));
  }

  void* PythonObject_CallFunction(void* raw_py_obj)
  {
    PyObject* casted_py_obj = castToPyObject(raw_py_obj);
    Py_INCREF(casted_py_obj);
    PyObject* return_value = PyObject_CallFunction(casted_py_obj, nullptr);
    Py_DECREF(casted_py_obj);
    return castToVoidPtr(return_value);
  }

  bool PythonObject_IsTrue(void* py_obj_raw)
  {
    return Py_IsTrue(castToPyObject(py_obj_raw));
  }

  unsigned long long PythonTuple_GetSize(void* raw_python_tuple)
  {
    return PyTuple_GET_SIZE(castToPyObject(raw_python_tuple));
  }

  void* PythonTuple_GetItem(void* raw_python_tuple, unsigned long long index)
  {
    return reinterpret_cast<void*>(PyTuple_GetItem(castToPyObject(raw_python_tuple), index));
  }

  bool PythonTuple_Check(void* raw_python_arg)
  {
    return PyTuple_Check(castToPyObject(raw_python_arg));
  }

  unsigned long long PythonLongObj_AsU64(void* raw_python_obj)
  {
    return PyLong_AsUnsignedLong(castToPyObject(raw_python_obj));
  }

  signed long long PythonLongObj_AsS64(void* raw_python_obj)
  {
    return PyLong_AsLongLong(castToPyObject(raw_python_obj));
  }

  void* S64_ToPythonLongObj(signed long long input_s64)
  {
    return castToVoidPtr(PyLong_FromLongLong(input_s64));
  }

  void* U64_ToPythonLongObj(unsigned long long input_u64)
  {
    return castToVoidPtr(PyLong_FromUnsignedLongLong(input_u64));
  }

  double PythonFloatObj_AsDouble(void* raw_python_obj)
  {
    return PyFloat_AsDouble(castToPyObject(raw_python_obj));
  }

  const char* PythonUnicodeObj_AsString(void* raw_python_obj)
  {
    return PyUnicode_AsUTF8(castToPyObject(raw_python_obj));
  }

  void* PythonDictionary_New()
  {
    return castToVoidPtr(PyDict_New());
  }

  void PythonDictionary_SetItem(void* raw_dict_obj, void* raw_key_obj, void* raw_val_obj)
  {
    PyDict_SetItem(castToPyObject(raw_dict_obj), castToPyObject(raw_key_obj), castToPyObject(raw_val_obj));
  }

  PyObject* key = nullptr;
  PyObject* value = nullptr;

  void* ResetAndGetRef_ToPyKey()
  {
    key = nullptr;
    return castToVoidPtr(key);
  }

  void* ResetAndGetRef_ToPyVal()
  {
    value = nullptr;
    return castToVoidPtr(value);
  }

  // Since everything in Python has a global lock, that means that only one python function can be executing at a time.
  // Also, functions cannot be interrupted by another thread/script once they've started, which means it's safe
  // to have a static value for the key and value that get passed into this function (via the above 2 functions)
  bool PythonDict_Next(void* raw_dict_obj, signed long long* ptr_to_index, void** raw_key_ptr_ptr, void** raw_value_ptr_ptr)
  {
    PyObject** casted_key = reinterpret_cast<PyObject**>(raw_key_ptr_ptr);
    PyObject** casted_value = reinterpret_cast<PyObject**>(raw_value_ptr_ptr);
    return PyDict_Next(castToPyObject(raw_dict_obj), ptr_to_index, casted_key, casted_value);
  }

  unsigned long long PythonList_Size(void* raw_py_obj)
  {
    return PyList_Size(castToPyObject(raw_py_obj));
  }

  void* PythonList_GetItem(void* raw_py_list, unsigned long long index)
  {
    return castToVoidPtr(PyList_GetItem(castToPyObject(raw_py_list), index));
  }

  bool PythonList_Check(void* raw_py_obj)
  {
    return PyList_Check(castToPyObject(raw_py_obj));
  }

  void* StringTo_PythonUnicodeObj(const char* input_str)
  {
    return castToVoidPtr(PyUnicode_FromString(input_str));
  }

  void* PythonBooleanObj_FromLong(long long input_long_long)
  {
    return PyBool_FromLong(input_long_long);
  }
}
