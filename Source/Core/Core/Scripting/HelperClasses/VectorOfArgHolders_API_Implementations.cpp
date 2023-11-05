#include "Core/Scripting/HelperClasses/VectorOfArgHolders_API_Implementations.h"

namespace Scripting
{

static std::vector<Scripting::ArgHolder*>* CastToVectorOfArgHolders(void* input)
{
  return reinterpret_cast<std::vector<Scripting::ArgHolder*>*>(input);
}

static Scripting::ArgHolder* CastToArgHolder(void* input)
{
  return reinterpret_cast<Scripting::ArgHolder*>(input);
}

void* CreateNewVectorOfArgHolders_impl()
{
  return reinterpret_cast<void*>(new std::vector<Scripting::ArgHolder*>());
}

unsigned long long GetSizeOfVectorOfArgHolders_impl(void* input)
{
  return CastToVectorOfArgHolders(input)->size();
}

void* GetArgumentForVectorOfArgHolders_impl(void* vectorOfArgHolders, unsigned int index)
{
  return (void*)((CastToVectorOfArgHolders(vectorOfArgHolders))->at(index));
}

void PushBack_VectorOfArgHolders_impl(void* vector_of_arg_holders, void* arg_holder)
{
  CastToVectorOfArgHolders(vector_of_arg_holders)->push_back(CastToArgHolder(arg_holder));
}

void Delete_VectorOfArgHolders_impl(void* vector_of_arg_holders)
{
  if (vector_of_arg_holders == nullptr)
    return;

  std::vector<Scripting::ArgHolder*>* casted_pointer =
      CastToVectorOfArgHolders(vector_of_arg_holders);
  std::vector<Scripting::ArgHolder*>& vector_reference = *(casted_pointer);
  size_t length = vector_reference.size();
  for (size_t i = 0; i < length; ++i)
  {
    delete (vector_reference[i]);
    vector_reference[i] = nullptr;
  }

  delete casted_pointer;
}

}  // namespace Scripting
