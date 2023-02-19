#include "rc_internal.h"

#include <stdlib.h>

static int rc_parse_operator(const char** memaddr) {
  const char* oper = *memaddr;

  switch (*oper) {
    case '=':
      ++(*memaddr);
      (*memaddr) += (**memaddr == '=');
      return RC_OPERATOR_EQ;

    case '!':
      if (oper[1] == '=') {
        (*memaddr) += 2;
        return RC_OPERATOR_NE;
      }
      /* fall through */
    default:
      return RC_INVALID_OPERATOR;

    case '<':
      if (oper[1] == '=') {
        (*memaddr) += 2;
        return RC_OPERATOR_LE;
      }

      ++(*memaddr);
      return RC_OPERATOR_LT;

    case '>':
      if (oper[1] == '=') {
        (*memaddr) += 2;
        return RC_OPERATOR_GE;
      }

      ++(*memaddr);
      return RC_OPERATOR_GT;

    case '*':
      ++(*memaddr);
      return RC_OPERATOR_MULT;

    case '/':
      ++(*memaddr);
      return RC_OPERATOR_DIV;

    case '&':
      ++(*memaddr);
      return RC_OPERATOR_AND;

    case '\0':/* end of string */
    case '_': /* next condition */
    case 'S': /* next condset */
    case ')': /* end of macro */
    case '$': /* maximum of values */
      /* valid condition separator, condition may not have an operator */
      return RC_OPERATOR_NONE;
  }
}

rc_condition_t* rc_parse_condition(const char** memaddr, rc_parse_state_t* parse, int is_indirect) {
  rc_condition_t* self;
  const char* aux;
  int result;
  int can_modify = 0;

  aux = *memaddr;
  self = RC_ALLOC(rc_condition_t, parse);
  self->current_hits = 0;
  self->is_true = 0;
  self->pause = 0;

  if (*aux != 0 && aux[1] == ':') {
    switch (*aux) {
      case 'p': case 'P': self->type = RC_CONDITION_PAUSE_IF; break;
      case 'r': case 'R': self->type = RC_CONDITION_RESET_IF; break;
      case 'a': case 'A': self->type = RC_CONDITION_ADD_SOURCE; can_modify = 1; break;
      case 'b': case 'B': self->type = RC_CONDITION_SUB_SOURCE; can_modify = 1; break;
      case 'c': case 'C': self->type = RC_CONDITION_ADD_HITS; break;
      case 'd': case 'D': self->type = RC_CONDITION_SUB_HITS; break;
      case 'n': case 'N': self->type = RC_CONDITION_AND_NEXT; break;
      case 'o': case 'O': self->type = RC_CONDITION_OR_NEXT; break;
      case 'm': case 'M': self->type = RC_CONDITION_MEASURED; break;
      case 'q': case 'Q': self->type = RC_CONDITION_MEASURED_IF; break;
      case 'i': case 'I': self->type = RC_CONDITION_ADD_ADDRESS; can_modify = 1; break;
      case 't': case 'T': self->type = RC_CONDITION_TRIGGER; break;
      case 'z': case 'Z': self->type = RC_CONDITION_RESET_NEXT_IF; break;
      case 'g': case 'G':
          parse->measured_as_percent = 1;
          self->type = RC_CONDITION_MEASURED;
          break;
      /* e f h j k l s u v w x y */
      default: parse->offset = RC_INVALID_CONDITION_TYPE; return 0;
    }

    aux += 2;
  }
  else {
    self->type = RC_CONDITION_STANDARD;
  }

  result = rc_parse_operand(&self->operand1, &aux, is_indirect, parse);
  if (result < 0) {
    parse->offset = result;
    return 0;
  }

  result = rc_parse_operator(&aux);
  if (result < 0) {
    parse->offset = result;
    return 0;
  }

  self->oper = (char)result;
  switch (self->oper) {
    case RC_OPERATOR_NONE:
      /* non-modifying statements must have a second operand */
      if (!can_modify) {
        /* measured does not require a second operand when used in a value */
        if (self->type != RC_CONDITION_MEASURED) {
          parse->offset = RC_INVALID_OPERATOR;
          return 0;
        }
      }

      /* provide dummy operand of '1' and no required hits */
      self->operand2.type = RC_OPERAND_CONST;
      self->operand2.value.num = 1;
      self->required_hits = 0;
      *memaddr = aux;
      return self;

    case RC_OPERATOR_MULT:
    case RC_OPERATOR_DIV:
    case RC_OPERATOR_AND:
      /* modifying operators are only valid on modifying statements */
      if (can_modify)
        break;
      /* fallthrough */

    default:
      /* comparison operators are not valid on modifying statements */
      if (can_modify) {
        switch (self->type) {
          case RC_CONDITION_ADD_SOURCE:
          case RC_CONDITION_SUB_SOURCE:
          case RC_CONDITION_ADD_ADDRESS:
            /* prevent parse errors on legacy achievements where a condition was present before changing the type */
            self->oper = RC_OPERATOR_NONE;
            break;

          default:
            parse->offset = RC_INVALID_OPERATOR;
            return 0;
        }
      }
      break;
  }

  result = rc_parse_operand(&self->operand2, &aux, is_indirect, parse);
  if (result < 0) {
    parse->offset = result;
    return 0;
  }

  if (self->oper == RC_OPERATOR_NONE) {
    /* if operator is none, explicitly clear out the right side */
    self->operand2.type = RC_OPERAND_CONST;
    self->operand2.value.num = 0;
  }

  if (*aux == '(') {
    char* end;
    self->required_hits = (unsigned)strtoul(++aux, &end, 10);

    if (end == aux || *end != ')') {
      parse->offset = RC_INVALID_REQUIRED_HITS;
      return 0;
    }

    /* if operator is none, explicitly clear out the required hits */
    if (self->oper == RC_OPERATOR_NONE)
      self->required_hits = 0;
    else
      parse->has_required_hits = 1;

    aux = end + 1;
  }
  else if (*aux == '.') {
    char* end;
    self->required_hits = (unsigned)strtoul(++aux, &end, 10);

    if (end == aux || *end != '.') {
      parse->offset = RC_INVALID_REQUIRED_HITS;
      return 0;
    }

    /* if operator is none, explicitly clear out the required hits */
    if (self->oper == RC_OPERATOR_NONE)
      self->required_hits = 0;
    else
      parse->has_required_hits = 1;

    aux = end + 1;
  }
  else {
    self->required_hits = 0;
  }

  *memaddr = aux;
  return self;
}

int rc_condition_is_combining(const rc_condition_t* self) {
  switch (self->type) {
    case RC_CONDITION_STANDARD:
    case RC_CONDITION_PAUSE_IF:
    case RC_CONDITION_RESET_IF:
    case RC_CONDITION_MEASURED_IF:
    case RC_CONDITION_TRIGGER:
    case RC_CONDITION_MEASURED:
      return 0;

    default:
      return 1;
  }
}

int rc_test_condition(rc_condition_t* self, rc_eval_state_t* eval_state) {
  rc_typed_value_t value1, value2;

  rc_evaluate_operand(&value1, &self->operand1, eval_state);
  if (eval_state->add_value.type != RC_VALUE_TYPE_NONE)
    rc_typed_value_add(&value1, &eval_state->add_value);

  rc_evaluate_operand(&value2, &self->operand2, eval_state);

  return rc_typed_value_compare(&value1, &value2, self->oper);
}

void rc_evaluate_condition_value(rc_typed_value_t* value, rc_condition_t* self, rc_eval_state_t* eval_state) {
  rc_typed_value_t amount;

  rc_evaluate_operand(value, &self->operand1, eval_state);
  rc_evaluate_operand(&amount, &self->operand2, eval_state);

  switch (self->oper) {
    case RC_OPERATOR_MULT:
      rc_typed_value_multiply(value, &amount);
      break;

    case RC_OPERATOR_DIV:
      rc_typed_value_divide(value, &amount);
      break;

    case RC_OPERATOR_AND:
      rc_typed_value_convert(value, RC_VALUE_TYPE_UNSIGNED);
      rc_typed_value_convert(&amount, RC_VALUE_TYPE_UNSIGNED);
      value->value.u32 &= amount.value.u32;
      break;
  }
}
