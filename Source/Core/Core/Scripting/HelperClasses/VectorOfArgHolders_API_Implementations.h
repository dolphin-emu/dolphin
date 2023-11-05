#pragma once

#include <vector>
#include "Core/Scripting/HelperClasses/ArgHolder.h"

// This file contains the implementations for the APIs in VectorOfArgHolders_APIs
namespace Scripting
{

void* CreateNewVectorOfArgHolders_impl();
unsigned long long GetSizeOfVectorOfArgHolders_impl(void*);
void* GetArgumentForVectorOfArgHolders_impl(void*, unsigned int);
void PushBack_VectorOfArgHolders_impl(void*, void*);

void Delete_VectorOfArgHolders_impl(void*);

}  // namespace Scripting
