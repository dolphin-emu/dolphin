
namespace Scripting
{
  const void* GetListOfDefaultModules_impl();
  const void* GetListOfNonDefaultModules_impl();
  unsigned long long GetSizeOfList_impl(const void*);
  const char* GetElementAtListIndex_impl(const void*, unsigned long long);
}
