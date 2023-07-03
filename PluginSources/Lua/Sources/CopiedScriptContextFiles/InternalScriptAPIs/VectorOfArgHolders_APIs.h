#ifndef VECTOR_OF_ARG_HOLDERS_APIS
#define VECTOR_OF_ARG_HOLDERS_APIS

#ifdef __cplusplus
extern "C" {
#endif

// IMPORTANT NOTE: CreateNewVectorOfArgHolders allocates a new Vector on the heap. As such,
// Delete_VectorOfArgHolders needs to be called when done using the vector by the DLL in order to
// prevent memory leaks. Also, if the Delete_VectorOfArgHolders() function is called, then the
// delete function for ArgHolders MAY NOT be called on individual ArgHolders that were added to the
// VectorOfArgHolders, as this would cause a double-free!
typedef struct VectorOfArgHolders_APIs
{
  // This function creates and returns an opaque handle, which
  // is really a pointer to a vector of ArgHolder* structs.
  // This function allocates memory dynamically on the heap.
  void* (*CreateNewVectorOfArgHolders)();

  // Returns the number of arguments stored in the VectorOfArgHolders which is passed in as input.
  unsigned long long (*GetSizeOfVectorOfArgHolders)(void*);

  // Returns an opaque handle for the ArgHolder* which is stored at the specified index in the
  // VectorOfArgHolders (index starts at 0).
  void* (*GetArgumentForVectorOfArgHolders)(void*, unsigned int);

  // 1st param is the VectorOfArgHolders*, and the 2nd param is the ArgHolder* that the user wants
  // to add to the vector. This function adds the ArgHolder* to the end of the vector.
  void (*PushBack)(void*, void*);

  // Deletes the VectorOfArgHolders* passed into it (including calling delete on each
  // individual ArgHolder*).
  void (*Delete_VectorOfArgHolders)(void*);
  // WARNING: The user may not call delete on an ArgHolder* directly if it has been added to a
  // VectorOfArgHolders. Instead, they need to use the above delete method, which frees up the
  // vector and all ArgHolders in it.

} VectorOfArgHolders_APIs;

#ifdef __cplusplus
}
#endif

#endif
