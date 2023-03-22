#ifndef BIT_MODULE_IMPORTER
#define BIT_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::BitModuleImporter
{
  PyObject* ImportModule(const std::string& api_version);

}
#endif
