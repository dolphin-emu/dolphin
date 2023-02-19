#ifndef INTERNAL_H
#define INTERNAL_H

#include "rc_runtime_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rc_scratch_string {
  char* value;
  struct rc_scratch_string* left;
  struct rc_scratch_string* right;
}
rc_scratch_string_t;

#define RC_ALLOW_ALIGN(T) struct __align_ ## T { char ch; T t; };
RC_ALLOW_ALIGN(rc_condition_t)
RC_ALLOW_ALIGN(rc_condset_t)
RC_ALLOW_ALIGN(rc_lboard_t)
RC_ALLOW_ALIGN(rc_memref_t)
RC_ALLOW_ALIGN(rc_operand_t)
RC_ALLOW_ALIGN(rc_richpresence_t)
RC_ALLOW_ALIGN(rc_richpresence_display_t)
RC_ALLOW_ALIGN(rc_richpresence_display_part_t)
RC_ALLOW_ALIGN(rc_richpresence_lookup_t)
RC_ALLOW_ALIGN(rc_richpresence_lookup_item_t)
RC_ALLOW_ALIGN(rc_scratch_string_t)
RC_ALLOW_ALIGN(rc_trigger_t)
RC_ALLOW_ALIGN(rc_value_t)
RC_ALLOW_ALIGN(char)

#define RC_ALIGNOF(T) (sizeof(struct __align_ ## T) - sizeof(T))
#define RC_OFFSETOF(o, t) (int)((char*)&(o.t) - (char*)&(o))

#define RC_ALLOC(t, p) ((t*)rc_alloc((p)->buffer, &(p)->offset, sizeof(t), RC_ALIGNOF(t), &(p)->scratch, RC_OFFSETOF((p)->scratch.objs, __ ## t)))
#define RC_ALLOC_SCRATCH(t, p) ((t*)rc_alloc_scratch((p)->buffer, &(p)->offset, sizeof(t), RC_ALIGNOF(t), &(p)->scratch, RC_OFFSETOF((p)->scratch.objs, __ ## t)))

typedef struct rc_scratch_buffer {
  struct rc_scratch_buffer* next;
  int offset;
  unsigned char buffer[512 - 16];
}
rc_scratch_buffer_t;

typedef struct {
  rc_scratch_buffer_t buffer;
  rc_scratch_string_t* strings;

  struct objs {
    rc_condition_t* __rc_condition_t;
    rc_condset_t* __rc_condset_t;
    rc_lboard_t* __rc_lboard_t;
    rc_memref_t* __rc_memref_t;
    rc_operand_t* __rc_operand_t;
    rc_richpresence_t* __rc_richpresence_t;
    rc_richpresence_display_t* __rc_richpresence_display_t;
    rc_richpresence_display_part_t* __rc_richpresence_display_part_t;
    rc_richpresence_lookup_t* __rc_richpresence_lookup_t;
    rc_richpresence_lookup_item_t* __rc_richpresence_lookup_item_t;
    rc_scratch_string_t __rc_scratch_string_t;
    rc_trigger_t* __rc_trigger_t;
    rc_value_t* __rc_value_t;
  } objs;
}
rc_scratch_t;

enum {
  RC_VALUE_TYPE_NONE,
  RC_VALUE_TYPE_UNSIGNED,
  RC_VALUE_TYPE_SIGNED,
  RC_VALUE_TYPE_FLOAT
};

typedef struct {
  union {
    unsigned u32;
    int i32;
    float f32;
  } value;

  char type;
}
rc_typed_value_t;

typedef struct {
  rc_typed_value_t add_value;/* AddSource/SubSource */
  int add_hits;             /* AddHits */
  unsigned add_address;     /* AddAddress */

  rc_peek_t peek;
  void* peek_userdata;
  lua_State* L;

  rc_typed_value_t measured_value;  /* Measured */
  char was_reset;           /* ResetIf triggered */
  char has_hits;            /* one of more hit counts is non-zero */
  char primed;              /* true if all non-Trigger conditions are true */
  char measured_from_hits;  /* true if the measured_value came from a condition's hit count */
  char was_cond_reset;      /* ResetNextIf triggered */
}
rc_eval_state_t;

typedef struct {
  int offset;

  lua_State* L;
  int funcs_ndx;

  void* buffer;
  rc_scratch_t scratch;

  rc_memref_t** first_memref;
  rc_value_t** variables;

  unsigned measured_target;
  int lines_read;

  char has_required_hits;
  char measured_as_percent;
}
rc_parse_state_t;

void rc_init_parse_state(rc_parse_state_t* parse, void* buffer, lua_State* L, int funcs_ndx);
void rc_init_parse_state_memrefs(rc_parse_state_t* parse, rc_memref_t** memrefs);
void rc_init_parse_state_variables(rc_parse_state_t* parse, rc_value_t** variables);
void rc_destroy_parse_state(rc_parse_state_t* parse);

void* rc_alloc(void* pointer, int* offset, int size, int alignment, rc_scratch_t* scratch, int scratch_object_pointer_offset);
void* rc_alloc_scratch(void* pointer, int* offset, int size, int alignment, rc_scratch_t* scratch, int scratch_object_pointer_offset);
char* rc_alloc_str(rc_parse_state_t* parse, const char* text, int length);

rc_memref_t* rc_alloc_memref(rc_parse_state_t* parse, unsigned address, char size, char is_indirect);
int rc_parse_memref(const char** memaddr, char* size, unsigned* address);
void rc_update_memref_values(rc_memref_t* memref, rc_peek_t peek, void* ud);
void rc_update_memref_value(rc_memref_value_t* memref, unsigned value);
unsigned rc_get_memref_value(rc_memref_t* memref, int operand_type, rc_eval_state_t* eval_state);
char rc_memref_shared_size(char size);
unsigned rc_memref_mask(char size);
void rc_transform_memref_value(rc_typed_value_t* value, char size);

void rc_parse_trigger_internal(rc_trigger_t* self, const char** memaddr, rc_parse_state_t* parse);
int rc_trigger_state_active(int state);

rc_condset_t* rc_parse_condset(const char** memaddr, rc_parse_state_t* parse, int is_value);
int rc_test_condset(rc_condset_t* self, rc_eval_state_t* eval_state);
void rc_reset_condset(rc_condset_t* self);

rc_condition_t* rc_parse_condition(const char** memaddr, rc_parse_state_t* parse, int is_indirect);
int rc_test_condition(rc_condition_t* self, rc_eval_state_t* eval_state);
void rc_evaluate_condition_value(rc_typed_value_t* value, rc_condition_t* self, rc_eval_state_t* eval_state);
int rc_condition_is_combining(const rc_condition_t* self);

int rc_parse_operand(rc_operand_t* self, const char** memaddr, int is_indirect, rc_parse_state_t* parse);
void rc_evaluate_operand(rc_typed_value_t* value, rc_operand_t* self, rc_eval_state_t* eval_state);
int rc_operand_is_float_memref(const rc_operand_t* self);

void rc_parse_value_internal(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse);
int rc_evaluate_value_typed(rc_value_t* self, rc_typed_value_t* value, rc_peek_t peek, void* ud, lua_State* L);
void rc_reset_value(rc_value_t* self);
rc_value_t* rc_alloc_helper_variable(const char* memaddr, int memaddr_len, rc_parse_state_t* parse);
void rc_update_variables(rc_value_t* variable, rc_peek_t peek, void* ud, lua_State* L);

void rc_typed_value_convert(rc_typed_value_t* value, char new_type);
void rc_typed_value_add(rc_typed_value_t* value, const rc_typed_value_t* amount);
void rc_typed_value_multiply(rc_typed_value_t* value, const rc_typed_value_t* amount);
void rc_typed_value_divide(rc_typed_value_t* value, const rc_typed_value_t* amount);
int rc_typed_value_compare(const rc_typed_value_t* value1, const rc_typed_value_t* value2, char oper);
void rc_typed_value_from_memref_value(rc_typed_value_t* value, const rc_memref_value_t* memref);

int rc_format_typed_value(char* buffer, int size, const rc_typed_value_t* value, int format);

void rc_parse_lboard_internal(rc_lboard_t* self, const char* memaddr, rc_parse_state_t* parse);
int rc_lboard_state_active(int state);

void rc_parse_richpresence_internal(rc_richpresence_t* self, const char* script, rc_parse_state_t* parse);

#ifdef __cplusplus
}
#endif

#endif /* INTERNAL_H */
