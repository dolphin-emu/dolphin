#ifndef MEMORY_MODULE_IMPORTER
#define MEMORY_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::MemoryModuleImporter
{
PyMODINIT_FUNC PyInit_MemoryAPI();
}  // namespace Scripting::Python::MemoryModuleImporter
#endif
