#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/ImportModuleImporter.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/InternalAPIModules/ImportAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/HelperClasses/CommonModuleImporterHelper.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::ImportModuleImporter
{
static bool initialized_import_module_importer = false;
static std::string import_class_name;
static std::vector<FunctionMetadata> all_import_functions;
static FunctionMetadata* import_module_1_0_metadata = nullptr;
static FunctionMetadata* import_1_0_metadata = nullptr;
static FunctionMetadata* shutdown_1_0_metadata = nullptr;

static PyObject* python_import_module_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, import_class_name,
                                          import_module_1_0_metadata);
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
    return CommonModuleImporterHelper::DoImport(
        api_version, &initialized_import_module_importer, &import_class_name, &all_import_functions,
        ImportAPI::GetAllClassMetadata, ImportAPI::GetClassMetadataForVersion,

        {{python_import_module_1_0, ImportAPI::ImportModule, &import_module_1_0_metadata},
         {python_import_1_0, ImportAPI::ImportAlt, &import_1_0_metadata},
         {python_shutdown_1_0, ImportAPI::Shutdown, &shutdown_1_0_metadata}});
  }

}  // namespace Scripting::Python::ImportModuleImporter
