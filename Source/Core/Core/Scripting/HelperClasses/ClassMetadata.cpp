#include "Core/Scripting/HelperClasses/ClassMetadata.h"

namespace Scripting
{

ClassMetadata* castToClassMetadataPtr(void* input)
{
  return reinterpret_cast<ClassMetadata*>(input);
}

const char* GetClassName_ClassMetadata_impl(void* class_metadata)
{
  return castToClassMetadataPtr(class_metadata)->class_name.c_str();
}

unsigned long long GetNumberOfFunctions_ClassMetadata_impl(void* class_metadata)
{
  return castToClassMetadataPtr(class_metadata)->functions_list.size();
}

void* GetFunctionMetadataAtPosition_ClassMetadata_impl(void* class_metadata, unsigned int index)
{
  return reinterpret_cast<void*>(
      &(castToClassMetadataPtr(class_metadata)->functions_list.at(index)));
}

}  // namespace Scripting
