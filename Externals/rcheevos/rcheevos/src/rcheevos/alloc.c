#include "rc_internal.h"

#include <stdlib.h>
#include <string.h>

void* rc_alloc_scratch(void* pointer, int* offset, int size, int alignment, rc_scratch_t* scratch, int scratch_object_pointer_offset)
{
  rc_scratch_buffer_t* buffer;

  /* if we have a real buffer, then allocate the data there */
  if (pointer)
    return rc_alloc(pointer, offset, size, alignment, NULL, scratch_object_pointer_offset);

  /* update how much space will be required in the real buffer */
  {
    const int aligned_offset = (*offset + alignment - 1) & ~(alignment - 1);
    *offset += (aligned_offset - *offset);
    *offset += size;
  }

  /* find a scratch buffer to hold the temporary data */
  buffer = &scratch->buffer;
  do {
    const int aligned_buffer_offset = (buffer->offset + alignment - 1) & ~(alignment - 1);
    const int remaining = sizeof(buffer->buffer) - aligned_buffer_offset;

    if (remaining >= size) {
      /* claim the required space from an existing buffer */
      return rc_alloc(buffer->buffer, &buffer->offset, size, alignment, NULL, -1);
    }

    if (!buffer->next)
      break;

    buffer = buffer->next;
  } while (1);

  /* not enough space in any existing buffer, allocate more */
  if (size > (int)sizeof(buffer->buffer)) {
    /* caller is asking for more than we can fit in a standard rc_scratch_buffer_t.
     * leverage the fact that the buffer is the last field and extend its size.
     * this chunk will be exactly large enough to hold the needed data, and since offset
     * will exceed sizeof(buffer->buffer), it will never be eligible to hold anything else.
     */
    const int needed = sizeof(rc_scratch_buffer_t) - sizeof(buffer->buffer) + size;
    buffer->next = (rc_scratch_buffer_t*)malloc(needed);
  }
  else {
    buffer->next = (rc_scratch_buffer_t*)malloc(sizeof(rc_scratch_buffer_t));
  }

  if (!buffer->next) {
    *offset = RC_OUT_OF_MEMORY;
    return NULL;
  }

  buffer = buffer->next;
  buffer->offset = 0;
  buffer->next = NULL;

  /* claim the required space from the new buffer */
  return rc_alloc(buffer->buffer, &buffer->offset, size, alignment, NULL, -1);
}

void* rc_alloc(void* pointer, int* offset, int size, int alignment, rc_scratch_t* scratch, int scratch_object_pointer_offset) {
  void* ptr;

  *offset = (*offset + alignment - 1) & ~(alignment - 1);

  if (pointer != 0) {
    /* valid buffer, grab the next chunk */
    ptr = (void*)((char*)pointer + *offset);
  }
  else if (scratch != 0 && scratch_object_pointer_offset >= 0) {
    /* only allocate one instance of each object type (indentified by scratch_object_pointer_offset) */
    void** scratch_object_pointer = (void**)((char*)&scratch->objs + scratch_object_pointer_offset);
    ptr = *scratch_object_pointer;
    if (!ptr) {
      int used;
      ptr = *scratch_object_pointer = rc_alloc_scratch(NULL, &used, size, alignment, scratch, -1);
    }
  }
  else {
    /* nowhere to get memory from, return NULL */
    ptr = NULL;
  }

  *offset += size;
  return ptr;
}

char* rc_alloc_str(rc_parse_state_t* parse, const char* text, int length) {
  int used = 0;
  char* ptr;

  rc_scratch_string_t** next = &parse->scratch.strings;
  while (*next) {
    int diff = strncmp(text, (*next)->value, length);
    if (diff == 0) {
      diff = (*next)->value[length];
      if (diff == 0)
        return (*next)->value;
    }

    if (diff < 0)
      next = &(*next)->left;
    else
      next = &(*next)->right;
  }

  *next = (rc_scratch_string_t*)rc_alloc_scratch(NULL, &used, sizeof(rc_scratch_string_t), RC_ALIGNOF(rc_scratch_string_t), &parse->scratch, RC_OFFSETOF(parse->scratch.objs, __rc_scratch_string_t));
  ptr = (char*)rc_alloc_scratch(parse->buffer, &parse->offset, length + 1, RC_ALIGNOF(char), &parse->scratch, -1);

  if (!ptr || !*next) {
    if (parse->offset >= 0)
      parse->offset = RC_OUT_OF_MEMORY;

    return NULL;
  }

  memcpy(ptr, text, length);
  ptr[length] = '\0';

  (*next)->left = NULL;
  (*next)->right = NULL;
  (*next)->value = ptr;

  return ptr;
}

void rc_init_parse_state(rc_parse_state_t* parse, void* buffer, lua_State* L, int funcs_ndx)
{
  /* could use memset here, but rc_parse_state_t contains a 512 byte buffer that doesn't need to be initialized */
  parse->offset = 0;
  parse->L = L;
  parse->funcs_ndx = funcs_ndx;
  parse->buffer = buffer;
  parse->scratch.buffer.offset = 0;
  parse->scratch.buffer.next = NULL;
  parse->scratch.strings = NULL;
  memset(&parse->scratch.objs, 0, sizeof(parse->scratch.objs));
  parse->first_memref = 0;
  parse->variables = 0;
  parse->measured_target = 0;
  parse->lines_read = 0;
  parse->has_required_hits = 0;
  parse->measured_as_percent = 0;
}

void rc_destroy_parse_state(rc_parse_state_t* parse)
{
  rc_scratch_buffer_t* buffer = parse->scratch.buffer.next;
  rc_scratch_buffer_t* next;

  while (buffer) {
    next = buffer->next;
    free(buffer);
    buffer = next;
  }
}

const char* rc_error_str(int ret)
{
  switch (ret) {
    case RC_OK: return "OK";
    case RC_INVALID_LUA_OPERAND: return "Invalid Lua operand";
    case RC_INVALID_MEMORY_OPERAND: return "Invalid memory operand";
    case RC_INVALID_CONST_OPERAND: return "Invalid constant operand";
    case RC_INVALID_FP_OPERAND: return "Invalid floating-point operand";
    case RC_INVALID_CONDITION_TYPE: return "Invalid condition type";
    case RC_INVALID_OPERATOR: return "Invalid operator";
    case RC_INVALID_REQUIRED_HITS: return "Invalid required hits";
    case RC_DUPLICATED_START: return "Duplicated start condition";
    case RC_DUPLICATED_CANCEL: return "Duplicated cancel condition";
    case RC_DUPLICATED_SUBMIT: return "Duplicated submit condition";
    case RC_DUPLICATED_VALUE: return "Duplicated value expression";
    case RC_DUPLICATED_PROGRESS: return "Duplicated progress expression";
    case RC_MISSING_START: return "Missing start condition";
    case RC_MISSING_CANCEL: return "Missing cancel condition";
    case RC_MISSING_SUBMIT: return "Missing submit condition";
    case RC_MISSING_VALUE: return "Missing value expression";
    case RC_INVALID_LBOARD_FIELD: return "Invalid field in leaderboard";
    case RC_MISSING_DISPLAY_STRING: return "Missing display string";
    case RC_OUT_OF_MEMORY: return "Out of memory";
    case RC_INVALID_VALUE_FLAG: return "Invalid flag in value expression";
    case RC_MISSING_VALUE_MEASURED: return "Missing measured flag in value expression";
    case RC_MULTIPLE_MEASURED: return "Multiple measured targets";
    case RC_INVALID_MEASURED_TARGET: return "Invalid measured target";
    case RC_INVALID_COMPARISON: return "Invalid comparison";
    case RC_INVALID_STATE: return "Invalid state";
    case RC_INVALID_JSON: return "Invalid JSON";

    default: return "Unknown error";
  }
}
