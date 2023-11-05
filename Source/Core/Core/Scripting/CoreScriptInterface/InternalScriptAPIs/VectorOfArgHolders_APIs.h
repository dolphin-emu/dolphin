#pragma once

namespace Scripting
{
#ifdef __cplusplus
extern "C" {
#endif

// VectorOfArgHolders_APIs contains the APIs to create an opaque handle for a
// std::vector<ArgHolder*>*, and to add opaque ArgHolder*s to the std::vector<ArgHolder*>*
// This structure is then passed into FunctionMetadata_APIs's RunFunction API in order to pass
// arguments to script functions.

// Note that the first void* passed into each function represents a std::vector<ArgHolder*>* (which
// will be referred to as a VectorOfArgHolders for simplicity below) unless specified otherwise in
// the documentation.
typedef struct VectorOfArgHolders_APIs
{
  // IMPORTANT NOTE: CreateNewVectorOfArgHolders allocates a new Vector on the heap. As such,
  // Delete_VectorOfArgHolders needs to be called when the DLL is done using the vector in order to
  // prevent memory leaks. Also, if the Delete_VectorOfArgHolders() function is called, then the
  // delete function for ArgHolders MAY NOT be called on individual ArgHolder*s that were added to
  // the VectorOfArgHolders, as this would cause a double-free!

  // This function creates and returns an opaque handle to a pointer to a vector of ArgHolder*
  // structs (std::vector<ArgHolder*>*) This function allocates memory dynamically on the heap.
  void* (*CreateNewVectorOfArgHolders)();

  // Returns the number of arguments currently stored in the VectorOfArgHolders which is passed in
  // as input.
  unsigned long long (*GetSizeOfVectorOfArgHolders)(void*);

  // Returns an opaque handle for the ArgHolder* which is stored at the specified index in the
  // VectorOfArgHolders (index starts at 0).
  void* (*GetArgumentForVectorOfArgHolders)(void*, unsigned int);

  // 1st param is the VectorOfArgHolders, and the 2nd param is the ArgHolder* that the user wants
  // to add to the vector. This function adds the ArgHolder* to the end of the vector.
  void (*PushBack_VectorOfArgHolders)(void*, void*);

  // Deletes the VectorOfArgHolders passed into it (including calling delete on each
  // individual ArgHolder*).
  void (*Delete_VectorOfArgHolders)(void*);
  // WARNING: The user may not call delete on an ArgHolder* directly if it has been added to a
  // VectorOfArgHolders. Instead, they need to use the above delete method, which frees up the
  // vector and all ArgHolders in it.

} VectorOfArgHolders_APIs;

#ifdef __cplusplus
}
#endif

}  // namespace Scripting
