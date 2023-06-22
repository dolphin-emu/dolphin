#ifndef VECTOR_OF_ARG_HOLDERS_APIS
#define VECTOR_OF_ARG_HOLDERS_APIS

#ifdef __cplusplus
extern "C" {
#endif

// IMPORTANT NOTE: CreateNewVectorOfArgHolders allocates a new Vector on the heap. As such,
// Delete_VectorOfArgHolders needs to be called when done using the vector by the DLL in order to
// prevent memory leaks. Also, if the Delete_VectorOfArgHolders() function is called, then the
// delete function for ArgHolders MAY NOT be called on individual ArgHolders, as this would cause a
// double-free!
typedef struct VectorOfArgHolders_APIs
{
  void* (
      *CreateNewVectorOfArgHolders)();  // This function creates and returns an opaque handle, which
                                        // is really a pointer to a vector of ArgHolder* structs.
                                        // This function allocates memory dynamically on the heap.
  unsigned long long (*GetSizeOfVectorOfArgHolders)(
      void*);  // Returns the size of the VectorOfArgHolders which is passed in as input.
  void* (*GetArgumentForVectorOfArgHolders)(
      void*, unsigned int);  // Returns an opaque handle for the ArgHolder* which is stored at the
                             // specified index in the vector (index starts at 0).
  void (*PushBack)(
      void*,
      void*);  // 1st param is the VectorOfArgHolders*, 2nd param is the ArgHolder* that the user
               // wants to add to the vector. This function adds the ArgHolder* to the vector.

  void (*Delete_VectorOfArgHolders)(
      void*);  // Deletes the VectorOfArgHolders* passed into it (including calling delete on each
               // individual ArgHolder*).
  // WARNING: The user may not call delete on an ArgHolder directly if it has been added to a
  // VectorOfArgHolders. Instead, they need to use the above delete method, which frees up the
  // vector and all ArgHolders in it.
} VectorOfArgHolders_APIs;

#ifdef __cplusplus
}
#endif

#endif
