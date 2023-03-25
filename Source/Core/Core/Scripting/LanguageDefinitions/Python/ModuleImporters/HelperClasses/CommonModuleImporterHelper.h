#pragma once
#include <Python.h>
#include <string>
#include <vector>
#include "Core/Scripting/HelperClasses/ClassMetadata.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/HelperClasses/ImportHelperStruct.h"

namespace Scripting::Python::CommonModuleImporterHelper
{
PyMODINIT_FUNC DoImport(const std::string& api_version,
                        bool* already_initialized_main_functions_list_ptr,
                        std::string* class_name_ptr,
                        std::vector<FunctionMetadata>* list_of_all_functions_ptr,
                        ClassMetadata (*GetAllFunctions)(),
                        ClassMetadata (*GetAllFunctionsForVersion)(const std::string&),
                        std::vector<ImportHelperStruct> pyFuncAndMetadataPtrAndMetadataPtrPtr_List);
}
