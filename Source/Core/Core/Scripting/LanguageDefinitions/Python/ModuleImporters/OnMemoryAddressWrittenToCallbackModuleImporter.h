#ifndef ON_MEMORY_ADDRESS_WRITTEN_TO_MODULE_IMPORTER
#define ON_MEMORY_ADDRESS_WRITTEN_TO_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::OnMemoryAddressWrittenToCallbackModuleImporter
{
PyMODINIT_FUNC PyInit_OnMemoryAddressWrittenTo();
}
#endif
