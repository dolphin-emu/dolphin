#ifndef GRAPHICS_MODULE_IMPORTER
#define GRAPHICS_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::GraphicsModuleImporter
{
PyMODINIT_FUNC PyInit_GraphicsAPI();
}
#endif
