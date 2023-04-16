#ifndef INSTRUCTION_STEP_MODULE_IMPORTER
#define INSTRUCTION_STEP_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::InstructionStepModuleImporter
{
PyMODINIT_FUNC PyInit_InstructionStepAPI();
}  // namespace Scripting::Python::InstructionStepModuleImporter
#endif
