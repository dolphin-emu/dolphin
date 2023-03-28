#ifndef ON_FRAME_START_MODULE_IMPORTER
#define ON_FRAME_START_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::OnFrameStartCallbackModuleImporter
{
PyMODINIT_FUNC PyInit_OnFrameStart();
}
#endif
