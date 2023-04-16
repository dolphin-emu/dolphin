#include "Core/Scripting/HelperClasses/FunctionMetadata.h"
#include "Core/Scripting/InternalAPIModules/InstructionStepAPI.h"
#include "Core/Scripting/LanguageDefinitions/Python/ModuleImporters/InstructionStepModuleImporter.h"
#include "Core/Scripting/LanguageDefinitions/Python/PythonScriptContext.h"

namespace Scripting::Python::InstructionStepModuleImporter
{
static std::string instruction_step_class_name = InstructionStepAPI::class_name;
static const char* single_step_function_name = "singleStep";
static const char* get_instruction_from_address_function_name = "getInstructionFromAddress";

static PyObject* python_single_step(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, instruction_step_class_name,
                                          single_step_function_name);
}

static PyObject* python_get_instruction_from_address(PyObject* self, PyObject* args)
{
  return PythonScriptContext::RunFunction(self, args, instruction_step_class_name,
                                          get_instruction_from_address_function_name);
}

static PyMethodDef instruction_step_api_methods[] = {
    {single_step_function_name, python_single_step, METH_VARARGS, nullptr},
    {get_instruction_from_address_function_name, python_get_instruction_from_address, METH_VARARGS, nullptr},
    {nullptr, nullptr, 0, nullptr}};

static struct PyModuleDef InstructionStepAPImodule = {
    PyModuleDef_HEAD_INIT, instruction_step_class_name.c_str(), /* name of module */
    "InstructionStepAPI Module",                               /* module documentation, may be NULL */
    sizeof(std::string), /* size of per-interpreter state of the module, or -1 if the module keeps
                            state in global variables. */
    instruction_step_api_methods};

PyMODINIT_FUNC PyInit_InstructionStepAPI()
{
  return PyModule_Create(&InstructionStepAPImodule);
}

}  // namespace Scripting::Python::InstructionStepModuleImporter
