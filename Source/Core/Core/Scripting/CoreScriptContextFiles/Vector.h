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


void* Vector_Get(Vector* _this, int index)
{
  if (index < 0)
  {
    fprintf(stderr, "Error: Index cannot be an empty value for Vector_Get()\n");
    return NULL;
  }

  else if (index >= _this->length)
  {
    fprintf(stderr, "Error: Attempted to access an element passed the end of the vector in Vector_Get()\n");
    return NULL;
  }
  else
  {
    return _this->arr[index];
  }
}
    /* Appends value to end of vector  */
void Vector_PushBack(Vector* _this, void* ptr)
{
  if (_this->length == _this->size)
  {
    _this->arr = vec_realloc(_this->arr, 2 * (sizeof(void*) * _this->size));
    _this->size = 2 * _this->size;
  }
  _this->arr[_this->length++] = ptr;
}

void* Vector_Pop(Vector* _this)
{
  if (_this->length <= 0)
  {
    fprintf(stderr, "Error: cannot pop an empty vector for vec_pop()\n");
    return NULL;
  }

  void* value = _this->arr[_this->length - 1];

  _this->arr[_this->length - 1] = NULL;
  _this->length--;

  return value;
}

/* Inserts object at specified location */
int Vector_Insert(Vector* _this, void* ptr, int index)
{
  if (index == _this->length)
    Vector_PushBack(_this, ptr);
  else if (index > _this->length)
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
    if (_this->length == _this->size)
    {
      _this->arr = vec_realloc(_this->arr, 2 * (sizeof(void*) * _this->size));
      _this->size = 2 * _this->size;
    }

    int num_values_to_copy = _this->length - index - 1;   

    memmove(_this->arr + index + 1, _this->arr + index, num_values_to_copy * sizeof(void*));
    _this->arr[index] = ptr;
    _this->length++;

  }
  return 0;
}

/* Removes element from specified location */
void* Vector_RemoveByIndex(Vector* _this, int index)
{
  if (_this->length == 0)
  {
    fprintf(stderr, "Error: Cannot remove element from empty vector in vec_remove.\n");
    return NULL;
  }

  else if (index > _this->length - 1)
  {
    fprintf(stderr, "Error: Index out of bounds in vec_remove\n");
    return NULL;
  }

  else if (index < 0)
  {
    fprintf(stderr, "Error: Index into vector for vec_remove() may not be begative.\n");
    return NULL;
  }

  void* value = _this->arr[index];
  _this->arr[index] = NULL;

  if (index < _this->length - 1)
    memmove(_this->arr + index, _this->arr + index + 1, (_this->length - index - 1) * sizeof(void*));

  _this->length--;
  return value;
}

void* Vector_RemoveByValue(Vector* _this, void* value)
{
  if (_this->size <= 0 || _this->length <= 0)
    return NULL;

  int firstMatchingIndex = Vector_FindFirst(_this, value);

  if (firstMatchingIndex == -1)
    return NULL;

  return Vector_RemoveByIndex(_this, firstMatchingIndex);
}

int Vector_FindFirst(Vector* _this, void* value)
{
  if (_this->size <= 0 || _this->length <= 0)
    return -1;

  for (int i = 0; i < _this->length; ++i)
  {
    if (_this->arr[i] == value)
      return i;
  }
  return -1;
}

int Vector_Count(Vector* _this, void* value)
{
  if (_this->size <= 0 || _this->length <= 0)
    return 0;

  int count = 0;

  for (int i = 0; i < _this->length; ++i)
  {
    if (_this->arr[i] == value)
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

void Vector_Destructor(Vector* _this)
{
// If the destructor parameter passed into Vector_Initializer was NULL, then that means that this represents an array of primitive types which don't have a destructor.
  //Otherwise, the destructor_function represents the function that acts as the destructor for each object in the array, and each object in the array is the address of an object (we then invoke the destructor on each object).
  //In either case, we free the memory allocated for the array.
  if (_this->size <= 0)
    return;

  if (_this->destructor_function != NULL)
  {
    for (int i = 0; i < _this->length; ++i)
    {
      _this->destructor_function(_this->arr[i]);
    }
  }

  free(_this->arr);
}

#endif

#ifdef __cplusplus
}
#endif
