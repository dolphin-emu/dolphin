#ifndef GC_CONTROLLER_MODULE_IMPORTER
#define GC_CONTROLLER_MODULE_IMPORTE
#include <Python.h>
#include <string>

namespace Scripting::Python::GameCubeControllerModuleImporter
{
PyMODINIT_FUNC PyInit_GameCubeControllerAPI();
}
#endif
