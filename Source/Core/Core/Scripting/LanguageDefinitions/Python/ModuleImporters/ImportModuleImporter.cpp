#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/ImportModuleImporter.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/InternalAPIModules/ImportAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::ImportModuleImporter
{
static bool initialized_import_module_importer = false;
static std::string import_class_name;
static std::vector<FunctionMetadata> all_import_functions;
static FunctionMetadata* import_module_1_0_metadata = nullptr;
static FunctionMetadata* import_1_0_metadata = nullptr;
static FunctionMetadata* shutdown_1_0_metadata = nullptr;

void InitFunctionList()
{
  ClassMetadata classMetadata = ImportAPI::GetAllClassMetadata();
  import_class_name = classMetadata.class_name;
  all_import_functions = classMetadata.functions_list;
  int number_of_functions = (int)all_import_functions.size();
  for (int i = 0; i < number_of_functions; ++i)
  {
    FunctionMetadata* function_reference = &(all_import_functions[i]);

    if (all_import_functions[i].function_pointer == ImportAPI::ImportModule)
      import_module_1_0_metadata = function_reference;
    else if (all_import_functions[i].function_pointer == ImportAPI::ImportAlt)
      import_1_0_metadata = function_reference;
    else if (all_import_functions[i].function_pointer == ImportAPI::Shutdown)
      shutdown_1_0_metadata = function_reference;
     else
      {
        throw std::invalid_argument(
            "Unknown argument inside of ImportModuleImporter::InitFunctionsList(). Did you add a new "
            "function to the Import module and forget to update the list in this function?");
        return;
      }
  }
}

static PyObject* python_import_module_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, import_class_name, import_module_1_0_metadata);
}

static PyObject* python_import_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, import_class_name, import_1_0_metadata);
}

static PyObject* python_shutdown_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, import_class_name, shutdown_1_0_metadata);
}

PyMODINIT_FUNC ImportImportModule(const std::string& api_version)
{
  if (!initialized_import_module_importer)
  {
      InitFunctionList();
      initialized_import_module_importer = true;
  }

    std::vector<FunctionMetadata> functions_for_version = ImportAPI::GetClassMetadataForVersion(api_version).functions_list;

  std::vector<PyMethodDef> python_functions_to_register;

  size_t num_funcs_for_version = functions_for_version.size();
  for (size_t i = 0; i < num_funcs_for_version; ++i)
  {
      FunctionMetadata* functionMetadata = &functions_for_version[i];
      PyCFunction next_function_to_register = nullptr;
      if (functionMetadata->function_pointer == ImportAPI::ImportModule)
        next_function_to_register = python_import_module_1_0;
      else if (functionMetadata->function_pointer == ImportAPI::ImportAlt)
        next_function_to_register = python_import_1_0;
      else if (functionMetadata->function_pointer == ImportAPI::Shutdown)
        next_function_to_register = python_shutdown_1_0;

      if (next_function_to_register == nullptr)
      {
        PyErr_SetString(PyExc_RuntimeError,
            fmt::format("Unknown argument inside of ImportModuleImporter::ImportModule() for function "
                        "{}. Did you add a new "
                        "function to the Import module and forget to update the list in this function?",
                        functionMetadata->function_name)
                .c_str());
        return nullptr;
      }

      python_functions_to_register.push_back({functionMetadata->function_name.c_str(),
                                              next_function_to_register, METH_VARARGS,
                                              functionMetadata->example_function_call.c_str()});


  }
  python_functions_to_register.push_back({nullptr, nullptr, 0, nullptr});
    PyModuleDef module_obj = {PyModuleDef_HEAD_INIT, import_class_name.c_str(), "Import Module", 0,
                            &python_functions_to_register[0]};
  return PyModule_Create(&module_obj);
}
}
