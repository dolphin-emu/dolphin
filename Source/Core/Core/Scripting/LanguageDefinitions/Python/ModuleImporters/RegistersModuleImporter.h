#ifndef REGISTERS_MODULE_IMPORTER
#define REGISTERS_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::RegistersModuleImporter
{
PyMODINIT_FUNC PyInit_RegistersAPI();
}
#endif
