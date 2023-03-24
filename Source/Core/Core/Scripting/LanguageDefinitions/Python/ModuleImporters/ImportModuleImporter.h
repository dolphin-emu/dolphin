#ifndef IMPORT_MODULE_IMPORTER
#define IMPORT_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::ImportModuleImporter
{
  // It's turtles all the way down!!!!!!!!
PyMODINIT_FUNC ImportImportModule(const std::string& api_version);
}
#endif
