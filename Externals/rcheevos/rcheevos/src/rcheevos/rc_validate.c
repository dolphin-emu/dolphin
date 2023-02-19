#include "rc_validate.h"

#include "rc_compat.h"
#include "rc_internal.h"

#include <stddef.h>
#include <stdlib.h>

static unsigned rc_max_value(const rc_operand_t* operand)
{
  if (operand->type == RC_OPERAND_CONST)
    return operand->value.num;

  if (!rc_operand_is_memref(operand))
    return 0xFFFFFFFF;

  switch (operand->size) {
    case RC_MEMSIZE_BIT_0:
    case RC_MEMSIZE_BIT_1:
    case RC_MEMSIZE_BIT_2:
    case RC_MEMSIZE_BIT_3:
    case RC_MEMSIZE_BIT_4:
    case RC_MEMSIZE_BIT_5:
    case RC_MEMSIZE_BIT_6:
    case RC_MEMSIZE_BIT_7:
      return 1;

    case RC_MEMSIZE_LOW:
    case RC_MEMSIZE_HIGH:
      return 0xF;

    case RC_MEMSIZE_BITCOUNT:
      return 8;

    case RC_MEMSIZE_8_BITS:
      return (operand->type == RC_OPERAND_BCD) ? 165 : 0xFF;

    case RC_MEMSIZE_16_BITS:
    case RC_MEMSIZE_16_BITS_BE:
      return (operand->type == RC_OPERAND_BCD) ? 16665 : 0xFFFF;

    case RC_MEMSIZE_24_BITS:
    case RC_MEMSIZE_24_BITS_BE:
      return (operand->type == RC_OPERAND_BCD) ? 1666665 : 0xFFFFFF;

    default:
      return (operand->type == RC_OPERAND_BCD) ? 166666665 : 0xFFFFFFFF;
  }
}

static unsigned rc_scale_value(unsigned value, char oper, const rc_operand_t* operand)
{
  switch (oper) {
    case RC_OPERATOR_MULT:
    {
      unsigned long long scaled = ((unsigned long long)value) * rc_max_value(operand);
      if (scaled > 0xFFFFFFFF)
        return 0xFFFFFFFF;

      return (unsigned)scaled;
    }

    case RC_OPERATOR_DIV:
    {
      const unsigned min_val = (operand->type == RC_OPERAND_CONST) ? operand->value.num : 1;
      return value / min_val;
    }

    case RC_OPERATOR_AND:
      return rc_max_value(operand);

    default:
      return value;
  }
}

static int rc_validate_get_condition_index(const rc_condset_t* condset, const rc_condition_t* condition)
{
   int index = 1;
   const rc_condition_t* scan;
   for (scan = condset->conditions; scan != NULL; scan = scan->next)
   {
     if (scan == condition)
       return index;

     ++index;
   }

   return 0;
}

static int rc_validate_range(unsigned min_val, unsigned max_val, char oper, unsigned max, char result[], const size_t result_size) {
  switch (oper) {
    case RC_OPERATOR_AND:
      if (min_val > max) {
        snprintf(result, result_size, "Mask has more bits than source");
        return 0;
      }
      else if (min_val == 0 && max_val == 0) {
        snprintf(result, result_size, "Result of mask always 0");
        return 0;
      }
      break;

    case RC_OPERATOR_EQ:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      break;

    case RC_OPERATOR_NE:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      break;

    case RC_OPERATOR_GE:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      if (max_val == 0) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      break;

    case RC_OPERATOR_GT:
      if (min_val >= max) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      break;

    case RC_OPERATOR_LE:
      if (min_val >= max) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      break;

    case RC_OPERATOR_LT:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      if (max_val == 0) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      break;
  }

  return 1;
}

int rc_validate_condset(const rc_condset_t* condset, char result[], const size_t result_size, unsigned max_address) {
  const rc_condition_t* cond;
  unsigned max_val;
  int index = 1;
  unsigned long long add_source_max = 0;
  int in_add_hits = 0;
  int in_add_address = 0;
  int is_combining = 0;

  if (!condset) {
    *result = '\0';
    return 1;
  }

  for (cond = condset->conditions; cond; cond = cond->next, ++index) {
    unsigned max = rc_max_value(&cond->operand1);
    const int is_memref1 = rc_operand_is_memref(&cond->operand1);
    const int is_memref2 = rc_operand_is_memref(&cond->operand2);

    if (!in_add_address) {
      if (is_memref1 && cond->operand1.value.memref->address > max_address) {
        snprintf(result, result_size, "Condition %d: Address %04X out of range (max %04X)",
            index, cond->operand1.value.memref->address, max_address);
        return 0;
      }
      if (is_memref2 && cond->operand2.value.memref->address > max_address) {
        snprintf(result, result_size, "Condition %d: Address %04X out of range (max %04X)",
            index, cond->operand2.value.memref->address, max_address);
        return 0;
      }
    }
    else {
      in_add_address = 0;
    }

    switch (cond->type) {
      case RC_CONDITION_ADD_SOURCE:
        max = rc_scale_value(max, cond->oper, &cond->operand2);
        add_source_max += max;
        is_combining = 1;
        continue;

      case RC_CONDITION_SUB_SOURCE:
        max = rc_scale_value(max, cond->oper, &cond->operand2);
        if (add_source_max < max) /* potential underflow - may be expected */
          add_source_max = 0xFFFFFFFF;
        is_combining = 1;
        continue;

      case RC_CONDITION_ADD_ADDRESS:
        if (cond->operand1.type == RC_OPERAND_DELTA || cond->operand1.type == RC_OPERAND_PRIOR) {
          snprintf(result, result_size, "Condition %d: Using pointer from previous frame", index);
          return 0;
        }
        in_add_address = 1;
        is_combining = 1;
        continue;

      case RC_CONDITION_ADD_HITS:
      case RC_CONDITION_SUB_HITS:
        in_add_hits = 1;
        is_combining = 1;
        break;

      case RC_CONDITION_AND_NEXT:
      case RC_CONDITION_OR_NEXT:
      case RC_CONDITION_RESET_NEXT_IF:
        is_combining = 1;
        break;

      default:
        if (in_add_hits) {
          if (cond->required_hits == 0) {
            snprintf(result, result_size, "Condition %d: Final condition in AddHits chain must have a hit target", index);
            return 0;
          }

          in_add_hits = 0;
        }

        is_combining = 0;
        break;
    }

    /* if we're in an add source chain, check for overflow */
    if (add_source_max) {
      const unsigned long long overflow = add_source_max + max;
      if (overflow > 0xFFFFFFFFUL)
        max = 0xFFFFFFFF;
      else
        max += (unsigned)add_source_max;
    }

    /* check for comparing two differently sized memrefs */
    max_val = rc_max_value(&cond->operand2);
    if (max_val != max && add_source_max == 0 && is_memref1 && is_memref2) {
      snprintf(result, result_size, "Condition %d: Comparing different memory sizes", index);
      return 0;
    }

    /* if either side is a memref, or there's a running add source chain, check for impossible comparisons */
    if (is_memref1 || is_memref2 || add_source_max) {
      const size_t prefix_length = snprintf(result, result_size, "Condition %d: ", index);

      unsigned min_val;
      switch (cond->operand2.type) {
        case RC_OPERAND_CONST:
          min_val = cond->operand2.value.num;
          break;

        case RC_OPERAND_FP:
          min_val = (int)cond->operand2.value.dbl;

          /* cannot compare an integer memory reference to a non-integral floating point value */
          /* assert: is_memref1 (because operand2==FP means !is_memref2) */
          if (!add_source_max && !rc_operand_is_float_memref(&cond->operand1) &&
              (float)min_val != cond->operand2.value.dbl) {
            snprintf(result + prefix_length, result_size - prefix_length, "Comparison is never true");
            return 0;
          }

          break;

        default:
          min_val = 0;

          /* cannot compare an integer memory reference to a non-integral floating point value */
          /* assert: is_memref2 (because operand1==FP means !is_memref1) */
          if (cond->operand1.type == RC_OPERAND_FP && !add_source_max && !rc_operand_is_float_memref(&cond->operand2) &&
              (float)((int)cond->operand1.value.dbl) != cond->operand1.value.dbl) {
            snprintf(result + prefix_length, result_size - prefix_length, "Comparison is never true");
            return 0;
          }

          break;
      }

      if (!rc_validate_range(min_val, max_val, cond->oper, max, result + prefix_length, result_size - prefix_length))
        return 0;
    }

    add_source_max = 0;
  }

  if (is_combining) {
    snprintf(result, result_size, "Final condition type expects another condition to follow");
    return 0;
  }

  *result = '\0';
  return 1;
}

int rc_validate_memrefs(const rc_memref_t* memref, char result[], const size_t result_size, unsigned max_address) {
  if (max_address < 0xFFFFFFFF) {
    while (memref) {
      if (memref->address > max_address) {
        snprintf(result, result_size, "Address %04X out of range (max %04X)", memref->address, max_address);
        return 0;
      }

      memref = memref->next;
    }
  }

  return 1;
}

static int rc_validate_is_combining_condition(const rc_condition_t* condition)
{
  switch (condition->type)
  {
    case RC_CONDITION_ADD_ADDRESS:
    case RC_CONDITION_ADD_HITS:
    case RC_CONDITION_ADD_SOURCE:
    case RC_CONDITION_AND_NEXT:
    case RC_CONDITION_OR_NEXT:
    case RC_CONDITION_RESET_NEXT_IF:
    case RC_CONDITION_SUB_HITS:
    case RC_CONDITION_SUB_SOURCE:
      return 1;

    default:
      return 0;
  }
}

static const rc_condition_t* rc_validate_next_non_combining_condition(const rc_condition_t* condition)
{
  int is_combining = rc_validate_is_combining_condition(condition);
  for (condition = condition->next; condition != NULL; condition = condition->next)
  {
    if (rc_validate_is_combining_condition(condition))
      is_combining = 1;
    else if (is_combining)
      is_combining = 0;
    else
      return condition;
  }

  return NULL;
}

static int rc_validate_get_opposite_comparison(int oper)
{
  switch (oper)
  {
    case RC_OPERATOR_EQ: return RC_OPERATOR_NE;
    case RC_OPERATOR_NE: return RC_OPERATOR_EQ;
    case RC_OPERATOR_LT: return RC_OPERATOR_GE;
    case RC_OPERATOR_LE: return RC_OPERATOR_GT;
    case RC_OPERATOR_GT: return RC_OPERATOR_LE;
    case RC_OPERATOR_GE: return RC_OPERATOR_LT;
    default: return oper;
  }
}

static const rc_operand_t* rc_validate_get_comparison(const rc_condition_t* condition, int* comparison, unsigned* value)
{
  if (rc_operand_is_memref(&condition->operand1))
  {
    if (condition->operand2.type != RC_OPERAND_CONST)
      return NULL;

    *comparison = condition->oper;
    *value = condition->operand2.value.num;
    return &condition->operand1;
  }

  if (condition->operand1.type != RC_OPERAND_CONST)
    return NULL;

  if (!rc_operand_is_memref(&condition->operand2))
    return NULL;

  *comparison = rc_validate_get_opposite_comparison(condition->oper);
  *value = condition->operand1.value.num;
  return &condition->operand2;
}

enum {
  RC_OVERLAP_NONE = 0,
  RC_OVERLAP_CONFLICTING,
  RC_OVERLAP_REDUNDANT,
  RC_OVERLAP_DEFER
};

static int rc_validate_comparison_overlap(int comparison1, unsigned value1, int comparison2, unsigned value2)
{
  /* NOTE: this only cares if comp2 conflicts with comp1.
   * If comp1 conflicts with comp2, we'll catch that later (return RC_OVERLAP_NONE for now) */
  switch (comparison2)
  {
    case RC_OPERATOR_EQ:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a == 1 | a == 1 && a == 2 | a == 2 && a == 1 */
                              /*    redundant           conflict           conflict     */
           return (value1 == value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a == 1 | a <= 1 && a == 2 | a <= 2 && a == 1 */
                              /*      defer             conflict            defer       */
           return (value1 < value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GE: /* a >= 1 && a == 1 | a >= 1 && a == 2 | a >= 2 && a == 1 */
                              /*      defer              defer             conflict     */
           return (value1 > value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_NE: /* a != 1 && a == 1 | a != 1 && a == 2 | a != 2 && a == 1 */
                              /*     conflict            defer              defer       */
           return (value1 == value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_LT: /* a <  1 && a == 1 | a <  1 && a == 2 | a <  2 && a == 1 */
                              /*     conflict           conflict            defer       */
           return (value1 <= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GT: /* a >  1 && a == 1 | a >  1 && a == 2 | a >  2 && a == 1 */
                              /*     conflict            defer            conflict      */
           return (value1 >= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
      }
      break;

    case RC_OPERATOR_NE:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a != 1 | a == 1 && a != 2 | a == 2 && a != 1 */
                              /*    conflict           redundant           redundant    */
           return (value1 == value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_REDUNDANT;
         case RC_OPERATOR_LE: /* a <= 1 && a != 1 | a <= 1 && a != 2 | a <= 2 && a != 1 */
                              /*       none            redundant             none       */
           return (value1 < value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
         case RC_OPERATOR_GE: /* a >= 1 && a != 1 | a >= 1 && a != 2 | a >= 2 && a != 1 */
                              /*       none               none             redundant     */
           return (value1 > value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
         case RC_OPERATOR_NE: /* a != 1 && a != 1 | a != 1 && a != 2 | a != 2 && a != 1 */
                              /*     redundant            none               none       */
           return (value1 == value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a != 1 | a <  1 && a != 2 | a <  2 && a != 1 */
                              /*     redundant          redundant           none        */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
         case RC_OPERATOR_GT: /* a >  1 && a != 1 | a >  1 && a != 2 | a >  2 && a != 1 */
                              /*     redundant           none             redundant     */
           return (value1 >= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_NONE;
      }
      break;

    case RC_OPERATOR_LT:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a <  1 | a == 1 && a <  2 | a == 2 && a <  1 */
                              /*    conflict           redundant           conflict     */
           return (value1 < value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a <  1 | a <= 1 && a <  2 | a <= 2 && a <  1 */
                              /*      defer            redundant            defer       */
           return (value1 < value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GE: /* a >= 1 && a <  1 | a >= 1 && a <  2 | a >= 2 && a <  1 */
                              /*     conflict             none             conflict     */
           return (value1 >= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_NE: /* a != 1 && a <  1 | a != 1 && a <  2 | a != 2 && a <  1 */
                              /*      defer               none              defer       */
           return (value1 >= value2) ? RC_OVERLAP_DEFER : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a <  1 | a <  1 && a <  2 | a <  2 && a <  1 */
                              /*     redundant          redundant           defer       */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GT: /* a >  1 && a <  1 | a >  1 && a <  2 | a >  2 && a <  1 */
                              /*     conflict             none             conflict     */
           return (value1 >= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
      }
      break;

    case RC_OPERATOR_LE:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a <= 1 | a == 1 && a <= 2 | a == 2 && a <= 1 */
                              /*    redundant           redundant          conflict     */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a <= 1 | a <= 1 && a <= 2 | a <= 2 && a <= 1 */
                              /*    redundant           redundant            defer       */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GE: /* a >= 1 && a <= 1 | a >= 1 && a <= 2 | a >= 2 && a <= 1 */
                              /*       none               none             conflict     */
           return (value1 > value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_NE: /* a != 1 && a <= 1 | a != 1 && a <= 2 | a != 2 && a <= 1 */
                              /*       none               none              defer       */
           return (value1 > value2) ? RC_OVERLAP_DEFER : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a <= 1 | a <  1 && a <= 2 | a <  2 && a <= 1 */
                              /*     redundant          redundant           defer       */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GT: /* a >  1 && a <= 1 | a >  1 && a <= 2 | a >  2 && a <= 1 */
                              /*     conflict             none             conflict     */
           return (value1 >= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
      }
      break;

    case RC_OPERATOR_GT:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a >  1 | a == 1 && a >  2 | a == 2 && a >  1 */
                              /*     conflict           conflict          redundant     */
           return (value1 > value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a >  1 | a <= 1 && a >  2 | a <= 2 && a >  1 */
                              /*     conflict           conflict            defer       */
           return (value1 <= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_DEFER;
         case RC_OPERATOR_GE: /* a >= 1 && a >  1 | a >= 1 && a >  2 | a >= 2 && a >  1 */
                              /*      defer              defer            redundant     */
           return (value1 > value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_NE: /* a != 1 && a >  1 | a != 1 && a >  2 | a != 2 && a >  1 */
                              /*      defer              defer               none       */
           return (value1 <= value2) ? RC_OVERLAP_DEFER : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a >  1 | a <  1 && a >  2 | a <  2 && a >  1 */
                              /*     conflict           conflict             none       */
           return (value1 <= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_GT: /* a >  1 && a >  1 | a >  1 && a >  2 | a >  2 && a >  1 */
                              /*    redundant            defer            redundant     */
           return (value1 >= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
      }
      break;

    case RC_OPERATOR_GE:
      switch (comparison1)    /* comp1     comp2    comp1     comp2    comp1     comp2  */
      {                       /*  value1 = value2    value1 < value2    value1 > value2 */
         case RC_OPERATOR_EQ: /* a == 1 && a >= 1 | a == 1 && a >= 2 | a == 2 && a >= 1 */
                              /*    redundant           conflict          redundant     */
           return (value1 >= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_CONFLICTING;
         case RC_OPERATOR_LE: /* a <= 1 && a >= 1 | a <= 1 && a >= 2 | a <= 2 && a >= 1 */
                              /*       none             conflict            none        */
           return (value1 < value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_GE: /* a >= 1 && a >= 1 | a >= 1 && a >= 2 | a >= 2 && a >= 1 */
                              /*    redundant          redundant            defer       */
           return (value1 <= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
         case RC_OPERATOR_NE: /* a != 1 && a >= 1 | a != 1 && a >= 2 | a != 2 && a >= 1 */
                              /*       none              defer               none       */
           return (value1 < value2) ? RC_OVERLAP_DEFER : RC_OVERLAP_NONE;
         case RC_OPERATOR_LT: /* a <  1 && a >= 1 | a <  1 && a >= 2 | a <  2 && a >= 1 */
                              /*     conflict           conflict             none       */
           return (value1 <= value2) ? RC_OVERLAP_CONFLICTING : RC_OVERLAP_NONE;
         case RC_OPERATOR_GT: /* a >  1 && a >= 1 | a >  1 && a >= 2 | a >  2 && a >= 1 */
                              /*    redundant            defer            redundant     */
           return (value1 >= value2) ? RC_OVERLAP_REDUNDANT : RC_OVERLAP_DEFER;
      }
      break;
  }

  return RC_OVERLAP_NONE;
}

static int rc_validate_conflicting_conditions(const rc_condset_t* conditions, const rc_condset_t* compare_conditions,
    const char* prefix, const char* compare_prefix, char result[], const size_t result_size)
{
  int comparison1, comparison2;
  unsigned value1, value2;
  const rc_operand_t* operand1;
  const rc_operand_t* operand2;
  const rc_condition_t* compare_condition;
  const rc_condition_t* condition;
  int overlap;

  /* empty group */
  if (conditions == NULL || compare_conditions == NULL)
    return 1;

  /* outer loop is the source conditions */
  for (condition = conditions->conditions; condition != NULL;
      condition = rc_validate_next_non_combining_condition(condition))
  {
    /* hits can be captured at any time, so any potential conflict will not be conflicting at another time */
    if (condition->required_hits)
      continue;

    operand1 = rc_validate_get_comparison(condition, &comparison1, &value1);
    if (!operand1)
      continue;

    switch (condition->type)
    {
      case RC_CONDITION_PAUSE_IF:
        if (conditions != compare_conditions)
          break;
        /* fallthrough */
      case RC_CONDITION_RESET_IF:
        comparison1 = rc_validate_get_opposite_comparison(comparison1);
        break;
      default:
        if (rc_validate_is_combining_condition(condition))
          continue;
        break;
    }

    /* inner loop is the potentially conflicting conditions */
    for (compare_condition = compare_conditions->conditions; compare_condition != NULL;
        compare_condition = rc_validate_next_non_combining_condition(compare_condition))
    {
      if (compare_condition == condition)
        continue;

      if (compare_condition->required_hits)
        continue;

      operand2 = rc_validate_get_comparison(compare_condition, &comparison2, &value2);
      if (!operand2 || operand2->value.memref->address != operand1->value.memref->address ||
          operand2->size != operand1->size || operand2->type != operand1->type)
        continue;

      switch (compare_condition->type)
      {
        case RC_CONDITION_PAUSE_IF:
          if (conditions != compare_conditions)
            break;
          /* fallthrough */
        case RC_CONDITION_RESET_IF:
          comparison2 = rc_validate_get_opposite_comparison(comparison2);
          break;
        default:
          if (rc_validate_is_combining_condition(compare_condition))
            continue;
          break;
      }

      overlap = rc_validate_comparison_overlap(comparison1, value1, comparison2, value2);
      switch (overlap)
      {
        case RC_OVERLAP_CONFLICTING:
          if (compare_condition->type == RC_CONDITION_PAUSE_IF || condition->type == RC_CONDITION_PAUSE_IF)
          {
            /* ignore PauseIf conflicts between groups, unless both conditions are PauseIfs */
            if (conditions != compare_conditions && compare_condition->type != condition->type)
              continue;
          }
          break;

        case RC_OVERLAP_REDUNDANT:
          if (compare_condition->type == RC_CONDITION_PAUSE_IF || condition->type == RC_CONDITION_PAUSE_IF)
          {
            /* ignore PauseIf redundancies between groups */
            if (conditions != compare_conditions)
              continue;

            /* if the PauseIf is less restrictive than the other condition, it's just a guard. ignore it */
            if (rc_validate_comparison_overlap(comparison2, value2, comparison1, value1) != RC_OVERLAP_REDUNDANT)
              continue;

            /* PauseIf redundant with ResetIf is a conflict (both are inverted comparisons) */
            if (compare_condition->type == RC_CONDITION_RESET_IF || condition->type == RC_CONDITION_RESET_IF)
              overlap = RC_OVERLAP_CONFLICTING;
          }
          else if (compare_condition->type == RC_CONDITION_RESET_IF && condition->type != RC_CONDITION_RESET_IF)
          {
            /* only ever report the redundancy on the non-ResetIf condition. The ResetIf is allowed to
             * fire when the non-ResetIf condition is not true. */
            continue;
          }
          else if (compare_condition->type == RC_CONDITION_MEASURED_IF || condition->type == RC_CONDITION_MEASURED_IF)
          {
            /* MeasuredIf is a meta tag and allowed to be redundant */
            if (compare_condition->type != condition->type)
              continue;
          }
          else if (compare_condition->type == RC_CONDITION_TRIGGER || condition->type == RC_CONDITION_TRIGGER)
          {
            /* Trigger is allowed to be redundant with non-trigger conditions as there may be limits that start a
             * challenge that are furhter reduced for the completion of the challenge */
            if (compare_condition->type != condition->type)
              continue;
          }
          break;

        default:
          continue;
      }

      if (compare_prefix && *compare_prefix)
      {
        snprintf(result, result_size, "%s Condition %d: %s with %s Condition %d",
            compare_prefix, rc_validate_get_condition_index(compare_conditions, compare_condition),
            (overlap == RC_OVERLAP_REDUNDANT) ? "Redundant" : "Conflicts",
            prefix, rc_validate_get_condition_index(conditions, condition));
      }
      else
      {
        snprintf(result, result_size, "Condition %d: %s with Condition %d",
            rc_validate_get_condition_index(compare_conditions, compare_condition),
            (overlap == RC_OVERLAP_REDUNDANT) ? "Redundant" : "Conflicts",
            rc_validate_get_condition_index(conditions, condition));
      }
      return 0;
    }
  }

  return 1;
}

int rc_validate_trigger(const rc_trigger_t* trigger, char result[], const size_t result_size, unsigned max_address) {
  const rc_condset_t* alt;
  int index;

  if (!trigger->alternative)
  {
    if (!rc_validate_condset(trigger->requirement, result, result_size, max_address))
      return 0;

    return rc_validate_conflicting_conditions(trigger->requirement, trigger->requirement, "", "", result, result_size);
  }

  snprintf(result, result_size, "Core ");
  if (!rc_validate_condset(trigger->requirement, result + 5, result_size - 5, max_address))
    return 0;

  /* compare core to itself */
  if (!rc_validate_conflicting_conditions(trigger->requirement, trigger->requirement, "Core", "Core", result, result_size))
    return 0;

  index = 1;
  for (alt = trigger->alternative; alt; alt = alt->next, ++index) {
    char altname[16];
    const size_t prefix_length = snprintf(result, result_size, "Alt%d ", index);
    if (!rc_validate_condset(alt, result + prefix_length, result_size - prefix_length, max_address))
      return 0;

    /* compare alt to itself */
    snprintf(altname, sizeof(altname), "Alt%d", index);
    if (!rc_validate_conflicting_conditions(alt, alt, altname, altname, result, result_size))
      return 0;

    /* compare alt to core */
    if (!rc_validate_conflicting_conditions(trigger->requirement, alt, "Core", altname, result, result_size))
      return 0;

    /* compare core to alt */
    if (!rc_validate_conflicting_conditions(alt, trigger->requirement, altname, "Core", result, result_size))
      return 0;
  }

  *result = '\0';
  return 1;
}

