#ifdef __cplusplus
extern "C" {
#endif

#ifndef C_VECTOR
#define C_VECTOR

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Malloc and Realloc with checks //
void** vec_malloc(size_t size);
void** vec_realloc(void* ptr, size_t size);

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

void* Vector_Get(Vector* __this, int index);
void Vector_PushBack(Vector* __this, void* ptr);
void* Vector_Pop(Vector* __this);
int Vector_Insert(Vector* __this, void* ptr, int index);
void* Vector_RemoveByIndex(Vector* __this, int index);
int Vector_FindFirst(Vector* __this, void* value);
void* Vector_RemoveByValue(Vector* __this, void* value);
int Vector_Count(Vector* __this, void* value);
Vector Vector_Initializer(void (*new_destructor_function)(void*));
void Vector_Destructor(Vector* __this);

#endif

#ifdef __cplusplus
}
#endif
