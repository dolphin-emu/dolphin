#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/HelperClasses/CommonModuleImporterHelper.h"

namespace Scripting::Python::CommonModuleImporterHelper
{
void InitFunctionsList(std::string api_version, std::string* class_name,
                       ClassMetadata (*GetAllFunctions)(),
                       std::vector<FunctionMetadata>* list_of_all_functions,
    std::vector<ImportHelperStruct> pyFuncAndMetadataPtrAndMetadataPtrPtr_List)
{
  ClassMetadata classMetadata = GetAllFunctions();
  *class_name = classMetadata.class_name;
  *list_of_all_functions = classMetadata.functions_list;
  size_t number_of_functions = list_of_all_functions->size();
  size_t number_of_metadata_helper_structs = pyFuncAndMetadataPtrAndMetadataPtrPtr_List.size();

  for (size_t i = 0; i < number_of_functions; ++i)
  {
    FunctionMetadata* function_reference = &((*list_of_all_functions)[i]);

    bool found_matching_function = false;
    for (size_t j = 0; j < number_of_metadata_helper_structs; ++j)
    {
      ImportHelperStruct* current_import_struct = &pyFuncAndMetadataPtrAndMetadataPtrPtr_List[j];
      if (current_import_struct->actual_api_function_ptr == function_reference->function_pointer)
      {
        *(current_import_struct->ptr_to_variable_that_stores_metadata) = function_reference;
        found_matching_function = true;
        break;
      }
    }
    if (!found_matching_function)
    {
      /* throw std::invalid_argument(fmt::format(
          "Unknown function encountered while registering module inside of "
          "CommonModuleHelper::ImportFunctionsList() for API {} and version {}. Did you add a new "
          "function to the {} and forget to update the list passed into the DoImport() function?",
          *class_name, api_version, *class_name));*/
      return;
    }
  }
}

PyMODINIT_FUNC DoImport(const std::string& api_version,
                        bool* already_initialized_main_functions_list_ptr,
                        std::string* class_name_ptr,
                        std::vector<FunctionMetadata>* list_of_all_functions_ptr,
                        ClassMetadata (*GetAllFunctions)(),
                        ClassMetadata (*GetAllFunctionsForVersion)(const std::string&),
         std::vector<ImportHelperStruct> pyFuncAndMetadataPtrAndMetadataPtrPtr_List)
{
  if (!(*already_initialized_main_functions_list_ptr))
  {
    InitFunctionsList(api_version, class_name_ptr, GetAllFunctions, list_of_all_functions_ptr,
                      pyFuncAndMetadataPtrAndMetadataPtrPtr_List);
    *already_initialized_main_functions_list_ptr = true;
  }

  std::vector<FunctionMetadata> functions_for_version =
      GetAllFunctionsForVersion(api_version).functions_list;
  std::vector<PyMethodDef> python_functions_to_register;
  size_t number_of_functions_for_version = functions_for_version.size();
  size_t total_number_of_python_functions = pyFuncAndMetadataPtrAndMetadataPtrPtr_List.size();
  for (size_t i = 0; i < number_of_functions_for_version; ++i)
  {
    FunctionMetadata* currentFunctionMetadata = &functions_for_version[i];
    PyCFunction next_function_to_register = nullptr;
    for (size_t j = 0; j < total_number_of_python_functions; ++j)
    {
      if (pyFuncAndMetadataPtrAndMetadataPtrPtr_List[j].actual_api_function_ptr ==
          currentFunctionMetadata->function_pointer)
      {
        next_function_to_register =
            pyFuncAndMetadataPtrAndMetadataPtrPtr_List[j].python_function_ptr;
        break;
      }
    }
    if (next_function_to_register == nullptr)
    {
      /* throw std::invalid_argument(fmt::format(
          "Unknown argument inside of CommonModuleImporter::DoImport() for version {} of "
          "function "
          "{} in class {}. Did you add a new "
          "function to {} and forget to update the list passed to this function?",
          api_version, currentFunctionMetadata->function_name, *class_name_ptr, *class_name_ptr));*/
      return nullptr;
    }

    python_functions_to_register.push_back(
        {currentFunctionMetadata->function_name.c_str(), next_function_to_register, METH_VARARGS,
         currentFunctionMetadata->example_function_call.c_str()});
  }

  python_functions_to_register.push_back({nullptr, nullptr, 0, nullptr});
  PyModuleDef module_obj = {PyModuleDef_HEAD_INIT, class_name_ptr->c_str(),
                            (std::string(class_name_ptr->c_str()) + " Module").c_str(), 0,
                            &python_functions_to_register[0]};
  return PyModule_Create(&module_obj);
}
}  // namespace Scripting::Python::CommonModuleImporterHelper
