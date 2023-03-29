#ifndef ON_GC_CONTROLLER_POLLED_CALLBACK_MODULE_IMPORTER
#define ON_GC_CONTROLLER_POLLED_CALLBACK_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::OnGCControllerPolledCallbackModuleImporter
{
PyMODINIT_FUNC PyInit_OnGCControllerPolled();
}
#endif
