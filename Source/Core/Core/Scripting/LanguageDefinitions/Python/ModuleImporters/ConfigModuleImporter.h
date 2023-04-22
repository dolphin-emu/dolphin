#ifndef CONFIG_MODULE_IMPORTER
#define CONFIG_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::ConfigModuleImporter
{
PyMODINIT_FUNC PyInit_ConfigAPI();
}
#endif
