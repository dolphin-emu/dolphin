#ifndef IMPORT_HELPER_STRUCT
#define IMPORT_HELPER_STRUCT
#include "Python.h"
#include "Core/Scripting/HelperClasses/FunctionMetadata.h"

namespace Scripting::Python
{
struct ImportHelperStruct
{
public:

  PyObject* (*python_function_ptr)(PyObject*, PyObject*);
  ArgHolder (*actual_api_function_ptr)(ScriptContext*, std::vector<ArgHolder>&);
  FunctionMetadata** ptr_to_variable_that_stores_metadata;
};

}
#endif
