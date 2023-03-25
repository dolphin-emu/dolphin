#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/BitModuleImporter.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/InternalAPIModules/BitAPI.h"

#include <vector>
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/HelperClasses/CommonModuleImporterHelper.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::BitModuleImporter
{
static bool initialized_bit_module_importer = false;
static std::string bit_class_name;
static std::vector<FunctionMetadata> all_bit_functions;
static FunctionMetadata* bitwise_and_1_0_metadata = nullptr;
static FunctionMetadata* bitwise_or_1_0_metadata = nullptr;
static FunctionMetadata* bitwise_not_1_0_metadata = nullptr;
static FunctionMetadata* bitwise_xor_1_0_metadata = nullptr;
static FunctionMetadata* logical_and_1_0_metadata = nullptr;
static FunctionMetadata* logical_or_1_0_metadata = nullptr;
static FunctionMetadata* logical_xor_1_0_metadata = nullptr;
static FunctionMetadata* logical_not_1_0_metadata = nullptr;
static FunctionMetadata* bit_shift_left_1_0_metadata = nullptr;
static FunctionMetadata* bit_shift_right_1_0_metadata = nullptr;

static PyObject* python_bitwise_and_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bitwise_and_1_0_metadata);
}

static PyObject* python_bitwise_or_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bitwise_or_1_0_metadata);
}

static PyObject* python_bitwise_not_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bitwise_not_1_0_metadata);
}

static PyObject* python_bitwise_xor_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bitwise_xor_1_0_metadata);
}

static PyObject* python_logical_and_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, logical_and_1_0_metadata);
}

static PyObject* python_logical_or_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, logical_or_1_0_metadata);
}

static PyObject* python_logical_xor_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, logical_xor_1_0_metadata);
}

static PyObject* python_logical_not_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, logical_not_1_0_metadata);
}

static PyObject* python_bit_shift_left_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bit_shift_left_1_0_metadata);
}

static PyObject* python_bit_shift_right_1_0(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, bit_class_name, bit_shift_right_1_0_metadata);
}

PyMODINIT_FUNC ImportBitModule(const std::string& api_version)
{
    return CommonModuleImporterHelper::DoImport(
        api_version, &initialized_bit_module_importer, &bit_class_name, &all_bit_functions,
        BitApi::GetAllClassMetadata, BitApi::GetClassMetadataForVersion,

        {{python_bitwise_and_1_0, BitApi::BitwiseAnd, &bitwise_and_1_0_metadata},
         {python_bitwise_or_1_0, BitApi::BitwiseOr, &bitwise_or_1_0_metadata},
         {python_bitwise_not_1_0, BitApi::BitwiseNot, &bitwise_not_1_0_metadata},
         {python_bitwise_xor_1_0, BitApi::BitwiseXor, &bitwise_xor_1_0_metadata},
         {python_logical_and_1_0, BitApi::LogicalAnd, &logical_and_1_0_metadata},
         {python_logical_or_1_0, BitApi::LogicalOr, &logical_or_1_0_metadata},
         {python_logical_xor_1_0, BitApi::LogicalXor, &logical_xor_1_0_metadata},
         {python_logical_not_1_0, BitApi::LogicalNot, &logical_not_1_0_metadata},
         {python_bit_shift_left_1_0, BitApi::BitShiftLeft, &bit_shift_left_1_0_metadata},
         {python_bit_shift_right_1_0, BitApi::BitShiftRight, &bit_shift_right_1_0_metadata}});
}

}  // namespace Scripting::Python::BitModuleImporter
