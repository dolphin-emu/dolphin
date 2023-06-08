#ifdef __cplusplus
extern "C" {
#endif

#ifndef C_VECTOR
#define C_VECTOR

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Malloc and Realloc with checks //
void** vec_malloc(size_t size)
{
  void* ret = malloc(size);
  if (ret == NULL)
  {
    fprintf(stderr, "malloc failed\n");
    exit(-1);
  }
  return reinterpret_cast<void**>(ret);
}

void** vec_realloc(void* ptr, size_t size)
{
  void* ret = realloc(ptr, size);
  if (ret == NULL)
  {
    fprintf(stderr, "realloc failed\n");
    exit(-1);
  }
  return reinterpret_cast<void**>(ret);
}

// Vector  struct //
typedef struct Vector
{
  int length;
  int size;
  void** arr;
  void (*destructor_function)(void*);
  void* (*get)(struct Vector*, int);
  void (*push_back)(struct Vector*, void*);
  void* (*pop)(struct Vector*);
  int (*insert)(struct Vector*, void*, int);
  void* (*remove_by_index)(struct Vector*, int);
  void* (*remove_by_value)(struct Vector*, void*);
  int (*find_first)(struct Vector*, void*);
  int (*count)(struct Vector*, void*);
} Vector;

// Vector Methods //


void* Vector_Get(Vector* __this, int index)
{
  if (index < 0)
  {
    fprintf(stderr, "Error: Index cannot be an empty value for Vector_Get()\n");
    return NULL;
  }

  else if (index >= __this->length)
  {
    fprintf(stderr, "Error: Attempted to access an element passed the end of the vector in Vector_Get()\n");
    return NULL;
  }
  else
  {
    return __this->arr[index];
  }
}
    /* Appends value to end of vector  */
void Vector_PushBack(Vector* __this, void* ptr)
{
  if (__this->length == __this->size)
  {
    __this->arr = vec_realloc(__this->arr, 2 * (sizeof(void*) * __this->size));
    __this->size = 2 * __this->size;
  }
  __this->arr[__this->length++] = ptr;
}

void* Vector_Pop(Vector* __this)
{
  if (__this->length <= 0)
  {
    fprintf(stderr, "Error: cannot pop an empty vector for vec_pop()\n");
    return NULL;
  }

  void* value = __this->arr[__this->length - 1];

  __this->arr[__this->length - 1] = NULL;
  __this->length--;

  return value;
}

/* Inserts object at specified location */
int Vector_Insert(Vector* __this, void* ptr, int index)
{
  if (index == __this->length)
    Vector_PushBack(__this, ptr);
  else if (index > __this->length)
  {
    fprintf(stderr, "Error: Index out of bounds for vec_insert()\n");
    return -1;
  }
  else if (index < 0)
  {
    fprintf(stderr, "Error: Index into vector for vec_insert() may not be negative.\n");
    return -1;
  }
  else
  {
    if (__this->length == __this->size)
    {
      __this->arr = vec_realloc(__this->arr, 2 * (sizeof(void*) * __this->size));
      __this->size = 2 * __this->size;
    }

    int num_values_to_copy = __this->length - index - 1;   

    memmove(__this->arr + index + 1, __this->arr + index, num_values_to_copy * sizeof(void*));
    __this->arr[index] = ptr;
    __this->length++;

  }
  return 0;
}

/* Removes element from specified location */
void* Vector_RemoveByIndex(Vector* __this, int index)
{
  if (__this->length == 0)
  {
    fprintf(stderr, "Error: Cannot remove element from empty vector in vec_remove.\n");
    return NULL;
  }

  else if (index > __this->length - 1)
  {
    fprintf(stderr, "Error: Index out of bounds in vec_remove\n");
    return NULL;
  }

  else if (index < 0)
  {
    fprintf(stderr, "Error: Index into vector for vec_remove() may not be begative.\n");
    return NULL;
  }

  void* value = __this->arr[index];
  __this->arr[index] = NULL;

  if (index < __this->length - 1)
    memmove(__this->arr + index, __this->arr + index + 1, (__this->length - index - 1) * sizeof(void*));

  __this->length--;
  return value;
}

int Vector_FindFirst(Vector* __this, void* value)
{
  if (__this->size <= 0 || __this->length <= 0)
    return -1;

  for (int i = 0; i < __this->length; ++i)
  {
    if (__this->arr[i] == value)
      return i;
  }
  return -1;
}

void* Vector_RemoveByValue(Vector* __this, void* value)
{
  if (__this->size <= 0 || __this->length <= 0)
    return NULL;

  int firstMatchingIndex = Vector_FindFirst(__this, value);

  if (firstMatchingIndex == -1)
    return NULL;

  return Vector_RemoveByIndex(__this, firstMatchingIndex);
}

int Vector_Count(Vector* __this, void* value)
{
  if (__this->size <= 0 || __this->length <= 0)
    return 0;

  int count = 0;

  for (int i = 0; i < __this->length; ++i)
  {
    if (__this->arr[i] == value)
      count++;
  }

  return count;
}

// Construction //

/* Constructs a default vector */
Vector Vector_Initializer(void (*new_destructor_function)(void*))
{
  int default_size = 16;
  Vector temp;
  temp.arr = vec_malloc(default_size * sizeof(void*));
  temp.length = 0;
  temp.size = default_size;
  temp.destructor_function = new_destructor_function;
  
  temp.get = Vector_Get;
  temp.push_back = Vector_PushBack;
  temp.pop = Vector_Pop;
  temp.insert = Vector_Insert;
  temp.remove_by_index = Vector_RemoveByIndex;
  temp.remove_by_value = Vector_RemoveByValue;
  temp.find_first = Vector_FindFirst;
  temp.count = Vector_Count;

  return temp;
}

// Destruction //

void Vector_Destructor(Vector* __this)
{
// If the destructor parameter passed into Vector_Initializer was NULL, then that means that this represents an array of primitive types which don't have a destructor.
  //Otherwise, the destructor_function represents the function that acts as the destructor for each object in the array, and each object in the array is the address of an object (we then invoke the destructor on each object).
  //In either case, we free the memory allocated for the array.
  if (__this->size <= 0)
    return;

  if (__this->destructor_function != NULL)
  {
    for (int i = 0; i < __this->length; ++i)
    {
      __this->destructor_function(__this->arr[i]);
    }
  }

  free(__this->arr);
  memset(__this, 0, sizeof(Vector));
}

#endif

#ifdef __cplusplus
}
#endif
