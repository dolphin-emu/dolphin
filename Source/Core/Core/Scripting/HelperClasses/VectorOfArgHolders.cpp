#include "Core/Scripting/HelperClasses/VectorOfArgHolders.h"

std::vector<Scripting::ArgHolder*>* castToVectorOfArgHolders(void* input)
{
  return reinterpret_cast<std::vector<Scripting::ArgHolder*>*>(input);
}

Scripting::ArgHolder* castToArgHolder(void* input)
{
  return reinterpret_cast<Scripting::ArgHolder*>(input);
}

void* CreateNewVectorOfArgHolders_impl()
{
  return reinterpret_cast<void*>(new std::vector<Scripting::ArgHolder*>());
}

unsigned long long GetSizeOfVectorOfArgHolders_impl(void* input)
{
  return castToVectorOfArgHolders(input)->size();
}

void* GetArgumentForVectorOfArgHolders_impl(void* vectorOfArgHolders, unsigned int index)
{
  return (void*)((castToVectorOfArgHolders(vectorOfArgHolders))->at(index));
}

void PushBack_VectorOfArgHolders_impl(void* vector_of_arg_holders, void* arg_holder)
{
  castToVectorOfArgHolders(vector_of_arg_holders)->push_back(castToArgHolder(arg_holder));
}

void Delete_VectorOfArgHolders_impl(void* vector_of_arg_holders)
{
  if (vector_of_arg_holders == nullptr)
    return;

  std::vector<Scripting::ArgHolder*>* casted_pointer =
      castToVectorOfArgHolders(vector_of_arg_holders);
  unsigned long long length = casted_pointer->size();
  for (unsigned long long i = 0; i < length; ++i)
  {
    delete (casted_pointer->at(i));
    casted_pointer->at(i) = nullptr;
  }

  delete casted_pointer;
}
