#ifndef STATISTICS_MODULE_IMPORTER
#define STATISTICS_MODULE_IMPORTER
#include <Python.h>
#include <string>

namespace Scripting::Python::StatisticsModuleImporter
{
PyMODINIT_FUNC PyInit_StatisticsAPI();
}
#endif
