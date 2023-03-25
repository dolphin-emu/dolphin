#ifndef EMU_MODULE_IMPORTER
#define EMU_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::EmuModuleImporter
{
PyMODINIT_FUNC ImportEmuModule(const std::string& api_version);
}
#endif
