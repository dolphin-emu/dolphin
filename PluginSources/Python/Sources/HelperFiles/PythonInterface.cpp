#include "PythonInterface.h"
#include "Python.h"
#include "../PythonScriptContextFiles/PythonScriptContext.h"
#include <string>
#include <vector>


namespace PythonInterface
{
  const char* function_metadata_capsule_name = "functionMetadataCapsule";
  const char* THIS_MODULE_NAME = "ThisModuleNamesdbkhcjs";
  static const char* redirect_output_module_name = "RedirectStdOut";

  static PyObject* RunFunction(PyObject* self, PyObject* args)
  {
    FunctionMetadata* current_function_metadata = reinterpret_cast<FunctionMetadata*>(PyCapsule_GetPointer(self, function_metadata_capsule_name));
    return reinterpret_cast<PyObject*>(RunFunction_impl(current_function_metadata, reinterpret_cast<void*>(self), reinterpret_cast<void*>(args)));
  }

  void Python_IncRef(void* x) {
  }
  void Python_DecRef(void* x) {}
  void* GetNoneObject(void* x) { return nullptr; }
  void* GetPyTrueObject() { return nullptr; }
  void* GetPyFalseObject() { return nullptr; }
  void* Python_BuildValue(const char* x, void* y) { return nullptr; }

  void* Python_RunString(const char* string_to_run)
  {
    return Python_RunString(string_to_run);
  }

  void Python_RunFile(const char* file_name)
  {
    FILE* fp = fopen(file_name, "rb");
    PyRun_AnyFile(fp, nullptr);
    fclose(fp);
  }

  void Python_SetRunTimeError(const char* x) {}
  bool Python_ErrOccured() { return true; }
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

  bool SetThisModule(void* python_script_context)
  {
    PyObject* this_module = PyImport_ImportModule(THIS_MODULE_NAME);
    if (this_module == nullptr)
      return false;
    void* this_module_state = PyModule_GetState(this_module);
    if (this_module_state == nullptr)
      return false;

    *(reinterpret_cast<unsigned long long*>(this_module_state)) = reinterpret_cast<unsigned long long>(python_script_context);
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
}
