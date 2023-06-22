#ifndef VECTOR_ARG_HOLDERS_IMPL
#define VECTOR_ARG_HOLDERS_IMPL

#include <vector>
#include "Core/Scripting/HelperClasses/ArgHolder.h"

#ifdef __cplusplus
extern "C" {
#endif

void* CreateNewVectorOfArgHolders_impl();
unsigned long long GetSizeOfVectorOfArgHolders_impl(void*);
void* GetArgumentForVectorOfArgHolders_impl(void*, unsigned int);
void PushBack_VectorOfArgHolders_impl(void*, void*);

void Delete_VectorOfArgHolders_impl(void*);

#ifdef __cplusplus
}
#endif

#endif
