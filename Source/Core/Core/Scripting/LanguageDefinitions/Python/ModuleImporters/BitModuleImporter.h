#ifndef BIT_MODULE_IMPORTER
#define BIT_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::BitModuleImporter
{
PyMODINIT_FUNC PyInit_BitAPI();
}
#endif
