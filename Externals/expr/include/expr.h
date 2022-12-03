#ifndef EXPR_H
#define EXPR_H

#ifdef _MSC_VER
#pragma warning(push)
// Disable warning for zero-sized array:
#pragma warning(disable : 4200)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <ctype.h> /* for isspace */
#include <limits.h>
#include <math.h> /* for pow */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Simple expandable vector implementation
 */
static int vec_expand(char **buf, int *length, int *cap, int memsz) {
  if (*length + 1 > *cap) {
    void *ptr;
    int n = (*cap == 0) ? 1 : *cap << 1;
    ptr = realloc(*buf, n * memsz);
    if (ptr == NULL) {
      return -1; /* allocation failed */
    }
    *buf = (char *)ptr;
    *cap = n;
  }
  return 0;
}
#define vec(T)                                                                 \
  struct {                                                                     \
    T *buf;                                                                    \
    int len;                                                                   \
    int cap;                                                                   \
  }
#define vec_init()                                                             \
  { NULL, 0, 0 }
#define vec_len(v) ((v)->len)
#define vec_unpack(v)                                                          \
  (char **)&(v)->buf, &(v)->len, &(v)->cap, sizeof(*(v)->buf)
#define vec_push(v, val)                                                       \
  vec_expand(vec_unpack(v)) ? -1 : ((v)->buf[(v)->len++] = (val), 0)
#define vec_nth(v, i) (v)->buf[i]
#define vec_peek(v) (v)->buf[(v)->len - 1]
#define vec_pop(v) (v)->buf[--(v)->len]
#define vec_free(v) (free((v)->buf), (v)->buf = NULL, (v)->len = (v)->cap = 0)
#define vec_foreach(v, var, iter)                                              \
  if ((v)->len > 0)                                                            \
    for ((iter) = 0; (iter) < (v)->len && (((var) = (v)->buf[(iter)]), 1);     \
         ++(iter))

/*
 * Expression data types
 */
struct expr;
struct expr_func;

enum expr_type {
  OP_UNKNOWN,
  OP_UNARY_MINUS,
  OP_UNARY_LOGICAL_NOT,
  OP_UNARY_BITWISE_NOT,

  OP_POWER,
  OP_DIVIDE,
  OP_MULTIPLY,
  OP_REMAINDER,

  OP_PLUS,
  OP_MINUS,

  OP_SHL,
  OP_SHR,

  OP_LT,
  OP_LE,
  OP_GT,
  OP_GE,
  OP_EQ,
  OP_NE,

  OP_BITWISE_AND,
  OP_BITWISE_OR,
  OP_BITWISE_XOR,

  OP_LOGICAL_AND,
  OP_LOGICAL_OR,

  OP_ASSIGN,
  OP_COMMA,

  OP_CONST,
  OP_VAR,
  OP_STRING,
  OP_FUNC,
};

static int prec[] = {0, 1, 1, 1, 2, 2, 2, 2,  3,  3,  4, 4, 5, 5, 5,
                     5, 5, 5, 6, 7, 8, 9, 10, 11, 12, 0, 0, 0, 0};

typedef vec(struct expr) vec_expr_t;
typedef void (*exprfn_cleanup_t)(struct expr_func *f, void *context);
typedef double (*exprfn_t)(struct expr_func *f, vec_expr_t *args,
                           void *context);

struct expr {
  enum expr_type type;
  union {
    struct {
      double value;
    } num;
    struct {
      double *value;
    } var;
    struct {
      vec_expr_t args;
    } op;
    struct {
      char *s;
    } str;
    struct {
      struct expr_func *f;
      vec_expr_t args;
      void *context;
    } func;
  } param;
};

#define expr_init()                                                            \
  { (enum expr_type)0 }

struct expr_string {
  const char *s;
  int n;
};
struct expr_arg {
  int oslen;
  int eslen;
  vec_expr_t args;
};

typedef vec(struct expr_string) vec_str_t;
typedef vec(struct expr_arg) vec_arg_t;

static int expr_is_unary(enum expr_type op) {
  return op == OP_UNARY_MINUS || op == OP_UNARY_LOGICAL_NOT ||
         op == OP_UNARY_BITWISE_NOT;
}

static int expr_is_binary(enum expr_type op) {
  return !expr_is_unary(op) && op != OP_CONST && op != OP_VAR &&
         op != OP_FUNC && op != OP_UNKNOWN && op != OP_STRING;
}

static int expr_prec(enum expr_type a, enum expr_type b) {
  int left =
      expr_is_binary(a) && a != OP_ASSIGN && a != OP_POWER && a != OP_COMMA;
  return (left && prec[a] >= prec[b]) || (prec[a] > prec[b]);
}

#define isfirstvarchr(c)                                                       \
  (((unsigned char)c >= '@' && c != '^' && c != '|' && c != '~') || c == '$')
#define isvarchr(c)                                                            \
  (((unsigned char)c >= '@' && c != '^' && c != '|' && c != '~') ||            \
   c == '$' || c == '#' || (c >= '0' && c <= '9'))

static struct {
  const char *s;
  const enum expr_type op;
} OPS[] = {
    {"-u", OP_UNARY_MINUS},
    {"!u", OP_UNARY_LOGICAL_NOT},
    {"~u", OP_UNARY_BITWISE_NOT},
    {"**", OP_POWER},
    {"*", OP_MULTIPLY},
    {"/", OP_DIVIDE},
    {"%", OP_REMAINDER},
    {"+", OP_PLUS},
    {"-", OP_MINUS},
    {"<<", OP_SHL},
    {">>", OP_SHR},
    {"<", OP_LT},
    {"<=", OP_LE},
    {">", OP_GT},
    {">=", OP_GE},
    {"==", OP_EQ},
    {"!=", OP_NE},
    {"&", OP_BITWISE_AND},
    {"|", OP_BITWISE_OR},
    {"^", OP_BITWISE_XOR},
    {"&&", OP_LOGICAL_AND},
    {"||", OP_LOGICAL_OR},
    {"=", OP_ASSIGN},
    {",", OP_COMMA},

    /* These are used by lexer and must be ignored by parser, so we put
       them at the end */
    {"-", OP_UNARY_MINUS},
    {"!", OP_UNARY_LOGICAL_NOT},
    {"~", OP_UNARY_BITWISE_NOT},
};

static enum expr_type expr_op(const char *s, size_t len, int unary) {
  for (unsigned int i = 0; i < sizeof(OPS) / sizeof(OPS[0]); i++) {
    if (strlen(OPS[i].s) == len && strncmp(OPS[i].s, s, len) == 0 &&
        (unary == -1 || expr_is_unary(OPS[i].op) == unary)) {
      return OPS[i].op;
    }
  }
  return OP_UNKNOWN;
}

static double expr_parse_number(const char *s, size_t len) {
  double num = 0;
  char buf[32];
  char *sz = buf;
  char *end = NULL;
  if (len >= sizeof(buf)) {
    sz = (char *)calloc(1, len + 1);
    if (sz == NULL) {
      return NAN;
    }
  }
  strncpy(sz, s, len);
  sz[len] = '\0';
  num = strtod(sz, &end);
  if (sz != buf) {
    free(sz);
  }
  return (end == sz + len ? num : NAN);
}

/*
 * Functions
 */
struct expr_func {
  const char *name;
  exprfn_t f;
  exprfn_cleanup_t cleanup;
  size_t ctxsz;
};

static struct expr_func *expr_get_func(struct expr_func *funcs, const char *s,
                                       size_t len) {
  for (struct expr_func *f = funcs; f->name; f++) {
    if (strlen(f->name) == len && strncmp(f->name, s, len) == 0) {
      return f;
    }
  }
  return NULL;
}

/*
 * Variables
 */
struct expr_var {
  double value;
  struct expr_var *next;
  char name[];
};

struct expr_var_list {
  struct expr_var *head;
};

static struct expr_var *expr_get_var(struct expr_var_list *vars, const char *s,
                                     size_t len) {
  struct expr_var *v = NULL;
  if (len == 0 || !isfirstvarchr(*s)) {
    return NULL;
  }
  for (v = vars->head; v; v = v->next) {
    if (strlen(v->name) == len && strncmp(v->name, s, len) == 0) {
      return v;
    }
  }
  v = (struct expr_var *)calloc(1, sizeof(struct expr_var) + len + 1);
  if (v == NULL) {
    return NULL; /* allocation failed */
  }
  v->next = vars->head;
  v->value = 0;
  strncpy(v->name, s, len);
  v->name[len] = '\0';
  vars->head = v;
  return v;
}

static int64_t to_int(double x) {
  if (isnan(x)) {
    return 0;
  } else if (isinf(x) != 0) {
    return INT64_MAX * isinf(x);
  } else {
    return (int64_t)x;
  }
}

static const char *expr_get_str(struct expr *e) {
  if (e->type != OP_STRING)
    return NULL;

  return e->param.str.s;
}

static double expr_eval(struct expr *e) {
  double n;
  switch (e->type) {
  case OP_UNARY_MINUS:
    return -(expr_eval(&e->param.op.args.buf[0]));
  case OP_UNARY_LOGICAL_NOT:
    return !(expr_eval(&e->param.op.args.buf[0]));
  case OP_UNARY_BITWISE_NOT:
    return ~(to_int(expr_eval(&e->param.op.args.buf[0])));
  case OP_POWER:
    return pow(expr_eval(&e->param.op.args.buf[0]),
               expr_eval(&e->param.op.args.buf[1]));
  case OP_MULTIPLY:
    return expr_eval(&e->param.op.args.buf[0]) *
           expr_eval(&e->param.op.args.buf[1]);
  case OP_DIVIDE:
    return expr_eval(&e->param.op.args.buf[0]) /
           expr_eval(&e->param.op.args.buf[1]);
  case OP_REMAINDER:
    return fmod(expr_eval(&e->param.op.args.buf[0]),
                expr_eval(&e->param.op.args.buf[1]));
  case OP_PLUS:
    return expr_eval(&e->param.op.args.buf[0]) +
           expr_eval(&e->param.op.args.buf[1]);
  case OP_MINUS:
    return expr_eval(&e->param.op.args.buf[0]) -
           expr_eval(&e->param.op.args.buf[1]);
  case OP_SHL:
    return to_int(expr_eval(&e->param.op.args.buf[0]))
           << to_int(expr_eval(&e->param.op.args.buf[1]));
  case OP_SHR:
    return to_int(expr_eval(&e->param.op.args.buf[0])) >>
           to_int(expr_eval(&e->param.op.args.buf[1]));
  case OP_LT:
    return expr_eval(&e->param.op.args.buf[0]) <
           expr_eval(&e->param.op.args.buf[1]);
  case OP_LE:
    return expr_eval(&e->param.op.args.buf[0]) <=
           expr_eval(&e->param.op.args.buf[1]);
  case OP_GT:
    return expr_eval(&e->param.op.args.buf[0]) >
           expr_eval(&e->param.op.args.buf[1]);
  case OP_GE:
    return expr_eval(&e->param.op.args.buf[0]) >=
           expr_eval(&e->param.op.args.buf[1]);
  case OP_EQ:
    return expr_eval(&e->param.op.args.buf[0]) ==
           expr_eval(&e->param.op.args.buf[1]);
  case OP_NE:
    return expr_eval(&e->param.op.args.buf[0]) !=
           expr_eval(&e->param.op.args.buf[1]);
  case OP_BITWISE_AND:
    return to_int(expr_eval(&e->param.op.args.buf[0])) &
           to_int(expr_eval(&e->param.op.args.buf[1]));
  case OP_BITWISE_OR:
    return to_int(expr_eval(&e->param.op.args.buf[0])) |
           to_int(expr_eval(&e->param.op.args.buf[1]));
  case OP_BITWISE_XOR:
    return to_int(expr_eval(&e->param.op.args.buf[0])) ^
           to_int(expr_eval(&e->param.op.args.buf[1]));
  case OP_LOGICAL_AND:
    n = expr_eval(&e->param.op.args.buf[0]);
    if (n != 0) {
      n = expr_eval(&e->param.op.args.buf[1]);
      if (n != 0) {
        return n;
      }
    }
    return 0;
  case OP_LOGICAL_OR:
    n = expr_eval(&e->param.op.args.buf[0]);
    if (n != 0 && !isnan(n)) {
      return n;
    } else {
      n = expr_eval(&e->param.op.args.buf[1]);
      if (n != 0) {
        return n;
      }
    }
    return 0;
  case OP_ASSIGN:
    n = expr_eval(&e->param.op.args.buf[1]);
    if (vec_nth(&e->param.op.args, 0).type == OP_VAR) {
      *e->param.op.args.buf[0].param.var.value = n;
    }
    return n;
  case OP_COMMA:
    expr_eval(&e->param.op.args.buf[0]);
    return expr_eval(&e->param.op.args.buf[1]);
  case OP_CONST:
    return e->param.num.value;
  case OP_VAR:
    return *e->param.var.value;
  case OP_FUNC:
    return e->param.func.f->f(e->param.func.f, &e->param.func.args,
                              e->param.func.context);
  default:
    return NAN;
  }
}

#define EXPR_TOP (1 << 0)
#define EXPR_TOPEN (1 << 1)
#define EXPR_TCLOSE (1 << 2)
#define EXPR_TLITERAL (1 << 3)
#define EXPR_TWORD (1 << 4)
#define EXPR_TDEFAULT (EXPR_TOPEN | EXPR_TLITERAL | EXPR_TWORD)

#define EXPR_UNARY (1 << 5)
#define EXPR_COMMA (1 << 6)

static int expr_next_token(const char *s, size_t len, int *flags) {
  unsigned int i = 0;
  if (len == 0) {
    return 0;
  }
  char c = s[0];
  if (c == '#') {
    for (; i < len && s[i] != '\n'; i++)
      ;
    return i;
  } else if (c == '\n') {
    for (; i < len && isspace(s[i]); i++)
      ;
    if (*flags & EXPR_TOP) {
      if (i == len || s[i] == ')') {
        *flags = *flags & (~EXPR_COMMA);
      } else {
        *flags = EXPR_TLITERAL | EXPR_TWORD | EXPR_TOPEN | EXPR_COMMA;
      }
    }
    return i;
  } else if (isspace(c)) {
    while (i < len && isspace(s[i]) && s[i] != '\n') {
      i++;
    }
    return i;
  } else if (isdigit(c)) {
    if ((*flags & EXPR_TLITERAL) == 0) {
      return -1; // unexpected number
    }
    *flags = EXPR_TOP | EXPR_TCLOSE;
    if (c == '0') {
      i++;
      if (i < len && (s[i] == 'x' || s[i] == 'X')) {
        i++;
        for (; i < len && isxdigit(s[i]); i++)
          ;
        return i;
      }
    }
    for (; i < len && (s[i] == '.' || isdigit(s[i])); i++)
      ;
    if (i < len && (s[i] == 'e' || s[i] == 'E')) {
      i++;
      if (i < len && (s[i] == '+' || s[i] == '-'))
        i++;
      for (; i < len && isdigit(s[i]); i++)
        ;
    }
    return i;
  } else if (c == '"') {
    if ((*flags & EXPR_TLITERAL) == 0) {
      return -6; // unexpected string
    }
    *flags = EXPR_TOP | EXPR_TCLOSE;
    i++;
    for (; i < len && s[i] != '"'; i++)
      ;
    if (i >= len) {
      return -7; // missing expected quote
    }
    i++;
    return i;
  } else if (isfirstvarchr(c)) {
    if ((*flags & EXPR_TWORD) == 0) {
      return -2; // unexpected word
    }
    *flags = EXPR_TOP | EXPR_TOPEN | EXPR_TCLOSE;
    while ((isvarchr(c)) && i < len) {
      i++;
      c = s[i];
    }
    return i;
  } else if (c == '(' || c == ')') {
    if (c == '(' && (*flags & EXPR_TOPEN) != 0) {
      *flags = EXPR_TLITERAL | EXPR_TWORD | EXPR_TOPEN | EXPR_TCLOSE;
    } else if (c == ')' && (*flags & EXPR_TCLOSE) != 0) {
      *flags = EXPR_TOP | EXPR_TCLOSE;
    } else {
      return -3; // unexpected parenthesis
    }
    return 1;
  } else {
    if ((*flags & EXPR_TOP) == 0) {
      if (expr_op(&c, 1, 1) == OP_UNKNOWN) {
        return -4; // missing expected operand
      }
      *flags = EXPR_TLITERAL | EXPR_TWORD | EXPR_TOPEN | EXPR_UNARY;
      return 1;
    } else {
      int found = 0;
      while (!isvarchr(c) && !isspace(c) && c != '(' && c != ')' && i < len) {
        if (expr_op(s, i + 1, 0) != OP_UNKNOWN) {
          found = 1;
        } else if (found) {
          break;
        }
        i++;
        c = s[i];
      }
      if (!found) {
        return -5; // unknown operator
      }
      *flags = EXPR_TLITERAL | EXPR_TWORD | EXPR_TOPEN;
      return i;
    }
  }
}

#define EXPR_PAREN_ALLOWED 0
#define EXPR_PAREN_EXPECTED 1
#define EXPR_PAREN_FORBIDDEN 2

static int expr_bind(const char *s, size_t len, vec_expr_t *es) {
  enum expr_type op = expr_op(s, len, -1);
  if (op == OP_UNKNOWN) {
    return -1;
  }

  if (expr_is_unary(op)) {
    if (vec_len(es) < 1) {
      return -1;
    }
    struct expr arg = vec_pop(es);
    struct expr unary = expr_init();
    unary.type = op;
    vec_push(&unary.param.op.args, arg);
    vec_push(es, unary);
  } else {
    if (vec_len(es) < 2) {
      return -1;
    }
    struct expr b = vec_pop(es);
    struct expr a = vec_pop(es);
    struct expr binary = expr_init();
    binary.type = op;
    if (op == OP_ASSIGN && a.type != OP_VAR) {
      return -1; /* Bad assignment */
    }
    vec_push(&binary.param.op.args, a);
    vec_push(&binary.param.op.args, b);
    vec_push(es, binary);
  }
  return 0;
}

static struct expr expr_const(double value) {
  struct expr e = expr_init();
  e.type = OP_CONST;
  e.param.num.value = value;
  return e;
}

static struct expr expr_varref(struct expr_var *v) {
  struct expr e = expr_init();
  e.type = OP_VAR;
  e.param.var.value = &v->value;
  return e;
}

static struct expr expr_binary(enum expr_type type, struct expr a,
                               struct expr b) {
  struct expr e = expr_init();
  e.type = type;
  vec_push(&e.param.op.args, a);
  vec_push(&e.param.op.args, b);
  return e;
}

static inline void expr_copy(struct expr *dst, struct expr *src) {
  int i;
  struct expr arg;
  dst->type = src->type;
  if (src->type == OP_FUNC) {
    dst->param.func.f = src->param.func.f;
    vec_foreach(&src->param.func.args, arg, i) {
      struct expr tmp = expr_init();
      expr_copy(&tmp, &arg);
      vec_push(&dst->param.func.args, tmp);
    }
    if (src->param.func.f->ctxsz > 0) {
      dst->param.func.context = calloc(1, src->param.func.f->ctxsz);
    }
  } else if (src->type == OP_CONST) {
    dst->param.num.value = src->param.num.value;
  } else if (src->type == OP_VAR) {
    dst->param.var.value = src->param.var.value;
  } else if (src->type == OP_STRING) {
    size_t len = strlen(src->param.str.s);
    dst->param.str.s = (char *)calloc(1, len + 1);
    if (dst->param.str.s != NULL) {
      strncpy(dst->param.str.s, src->param.str.s, len);
    }
  } else {
    vec_foreach(&src->param.op.args, arg, i) {
      struct expr tmp = expr_init();
      expr_copy(&tmp, &arg);
      vec_push(&dst->param.op.args, tmp);
    }
  }
}

static void expr_destroy_args(struct expr *e);

static struct expr *expr_create(const char *s, size_t len,
                                struct expr_var_list *vars,
                                struct expr_func *funcs) {
  double num;
  const char *id = NULL;
  size_t idn = 0;

  struct expr *result = NULL;

  vec_expr_t es = vec_init();
  vec_str_t os = vec_init();
  vec_arg_t as = vec_init();

  struct macro {
    char *name;
    vec_expr_t body;
  };
  vec(struct macro) macros = vec_init();

  int flags = EXPR_TDEFAULT;
  int paren = EXPR_PAREN_ALLOWED;
  for (;;) {
    int n = expr_next_token(s, len, &flags);
    if (n == 0) {
      break;
    } else if (n < 0) {
      goto cleanup;
    }
    const char *tok = s;
    s = s + n;
    len = len - n;
    if (*tok == '#') {
      continue;
    }
    if (flags & EXPR_UNARY) {
      if (n == 1) {
        switch (*tok) {
        case '-':
          tok = "-u";
          break;
        case '~':
          tok = "~u";
          break;
        case '!':
          tok = "!u";
          break;
        default:
          goto cleanup;
        }
        n = 2;
      }
    }
    if (*tok == '\n' && (flags & EXPR_COMMA)) {
      flags = flags & (~EXPR_COMMA);
      n = 1;
      tok = ",";
    }
    if (isspace(*tok)) {
      continue;
    }
    int paren_next = EXPR_PAREN_ALLOWED;

    if (idn > 0) {
      struct expr_var *v;
      if (n == 1 && *tok == '(') {
        int i;
        int has_macro = 0;
        struct macro m;
        vec_foreach(&macros, m, i) {
          if (strlen(m.name) == idn && strncmp(m.name, id, idn) == 0) {
            has_macro = 1;
            break;
          }
        }
        if ((idn == 1 && id[0] == '$') || has_macro ||
            expr_get_func(funcs, id, idn) != NULL) {
          struct expr_string str = {id, (int)idn};
          vec_push(&os, str);
          paren = EXPR_PAREN_EXPECTED;
        } else {
          goto cleanup; /* invalid function name */
        }
      } else if ((v = expr_get_var(vars, id, idn)) != NULL) {
        vec_push(&es, expr_varref(v));
        paren = EXPR_PAREN_FORBIDDEN;
      }
      id = NULL;
      idn = 0;
    }

    if (n == 1 && *tok == '(') {
      if (paren == EXPR_PAREN_EXPECTED) {
        struct expr_string str = {"{", 1};
        vec_push(&os, str);
        struct expr_arg arg = {vec_len(&os), vec_len(&es), vec_init()};
        vec_push(&as, arg);
      } else if (paren == EXPR_PAREN_ALLOWED) {
        struct expr_string str = {"(", 1};
        vec_push(&os, str);
      } else {
        goto cleanup; // Bad call
      }
    } else if (paren == EXPR_PAREN_EXPECTED) {
      goto cleanup; // Bad call
    } else if (n == 1 && *tok == ')') {
      int minlen = (vec_len(&as) > 0 ? vec_peek(&as).oslen : 0);
      while (vec_len(&os) > minlen && *vec_peek(&os).s != '(' &&
             *vec_peek(&os).s != '{') {
        struct expr_string str = vec_pop(&os);
        if (expr_bind(str.s, str.n, &es) == -1) {
          goto cleanup;
        }
      }
      if (vec_len(&os) == 0) {
        goto cleanup; // Bad parens
      }
      struct expr_string str = vec_pop(&os);
      if (str.n == 1 && *str.s == '{') {
        str = vec_pop(&os);
        struct expr_arg arg = vec_pop(&as);
        if (vec_len(&es) > arg.eslen) {
          vec_push(&arg.args, vec_pop(&es));
        }
        if (str.n == 1 && str.s[0] == '$') {
          if (vec_len(&arg.args) < 1) {
            vec_free(&arg.args);
            goto cleanup; /* too few arguments for $() function */
          }
          struct expr *u = &vec_nth(&arg.args, 0);
          if (u->type != OP_VAR) {
            vec_free(&arg.args);
            goto cleanup; /* first argument is not a variable */
          }
          for (struct expr_var *v = vars->head; v; v = v->next) {
            if (&v->value == u->param.var.value) {
              struct macro m = {v->name, arg.args};
              vec_push(&macros, m);
              break;
            }
          }
          vec_push(&es, expr_const(0));
        } else {
          int i = 0;
          int found = -1;
          struct macro m;
          vec_foreach(&macros, m, i) {
            if (strlen(m.name) == (size_t)str.n &&
                strncmp(m.name, str.s, str.n) == 0) {
              found = i;
            }
          }
          if (found != -1) {
            m = vec_nth(&macros, found);
            struct expr root = expr_const(0);
            struct expr *p = &root;
            /* Assign macro parameters */
            for (int j = 0; j < vec_len(&arg.args); j++) {
              char varname[12];
              snprintf(varname, sizeof(varname), "$%d", (j + 1));
              struct expr_var *v = expr_get_var(vars, varname, strlen(varname));
              struct expr ev = expr_varref(v);
              struct expr assign =
                  expr_binary(OP_ASSIGN, ev, vec_nth(&arg.args, j));
              *p = expr_binary(OP_COMMA, assign, expr_const(0));
              p = &vec_nth(&p->param.op.args, 1);
            }
            /* Expand macro body */
            for (int j = 1; j < vec_len(&m.body); j++) {
              if (j < vec_len(&m.body) - 1) {
                *p = expr_binary(OP_COMMA, expr_const(0), expr_const(0));
                expr_copy(&vec_nth(&p->param.op.args, 0), &vec_nth(&m.body, j));
              } else {
                expr_copy(p, &vec_nth(&m.body, j));
              }
              p = &vec_nth(&p->param.op.args, 1);
            }
            vec_push(&es, root);
            vec_free(&arg.args);
          } else {
            struct expr_func *f = expr_get_func(funcs, str.s, str.n);
            struct expr bound_func = expr_init();
            bound_func.type = OP_FUNC;
            bound_func.param.func.f = f;
            bound_func.param.func.args = arg.args;
            if (f->ctxsz > 0) {
              void *p = calloc(1, f->ctxsz);
              if (p == NULL) {
                goto cleanup; /* allocation failed */
              }
              bound_func.param.func.context = p;
            }
            vec_push(&es, bound_func);
          }
        }
      }
      paren_next = EXPR_PAREN_FORBIDDEN;
    } else if (!isnan(num = expr_parse_number(tok, n))) {
      vec_push(&es, expr_const(num));
      paren_next = EXPR_PAREN_FORBIDDEN;
    } else if (n > 1 && *tok == '"') {
      char *s = (char *)calloc(1, n - 1);
      if (s == NULL) {
        goto cleanup; /* allocation failed */
      }
      strncpy(s, tok + 1, n - 2);
      struct expr e = expr_init();
      e.type = OP_STRING;
      e.param.str.s = s;
      vec_push(&es, e);
      paren_next = EXPR_PAREN_FORBIDDEN;
    } else if (expr_op(tok, n, -1) != OP_UNKNOWN) {
      enum expr_type op = expr_op(tok, n, -1);
      struct expr_string o2 = {NULL, 0};
      if (vec_len(&os) > 0) {
        o2 = vec_peek(&os);
      }
      for (;;) {
        if (n == 1 && *tok == ',' && vec_len(&os) > 0) {
          struct expr_string str = vec_peek(&os);
          if (str.n == 1 && *str.s == '{') {
            struct expr e = vec_pop(&es);
            vec_push(&vec_peek(&as).args, e);
            break;
          }
        }
        enum expr_type type2 = expr_op(o2.s, o2.n, -1);
        if (!(type2 != OP_UNKNOWN && expr_prec(op, type2))) {
          struct expr_string str = {tok, n};
          vec_push(&os, str);
          break;
        }

        if (expr_bind(o2.s, o2.n, &es) == -1) {
          goto cleanup;
        }
        (void)vec_pop(&os);
        if (vec_len(&os) > 0) {
          o2 = vec_peek(&os);
        } else {
          o2.n = 0;
        }
      }
    } else {
      if (n > 0 && !isdigit(*tok)) {
        /* Valid identifier, a variable or a function */
        id = tok;
        idn = n;
      } else {
        goto cleanup; // Bad variable name, e.g. '2.3.4' or '4ever'
      }
    }
    paren = paren_next;
  }

  if (idn > 0) {
    vec_push(&es, expr_varref(expr_get_var(vars, id, idn)));
  }

  while (vec_len(&os) > 0) {
    struct expr_string rest = vec_pop(&os);
    if (rest.n == 1 && (*rest.s == '(' || *rest.s == ')')) {
      goto cleanup; // Bad paren
    }
    if (expr_bind(rest.s, rest.n, &es) == -1) {
      goto cleanup;
    }
  }

  result = (struct expr *)calloc(1, sizeof(struct expr));
  if (result != NULL) {
    if (vec_len(&es) == 0) {
      result->type = OP_CONST;
    } else {
      *result = vec_pop(&es);
    }
  }

  int i, j;
  struct macro m;
  struct expr e;
  struct expr_arg a;
cleanup:
  vec_foreach(&macros, m, i) {
    struct expr e2;
    vec_foreach(&m.body, e2, j) { expr_destroy_args(&e2); }
    vec_free(&m.body);
  }
  vec_free(&macros);

  vec_foreach(&es, e, i) { expr_destroy_args(&e); }
  vec_free(&es);

  vec_foreach(&as, a, i) {
    vec_foreach(&a.args, e, j) { expr_destroy_args(&e); }
    vec_free(&a.args);
  }
  vec_free(&as);

  /*vec_foreach(&os, o, i) {vec_free(&m.body);}*/
  vec_free(&os);
  return result;
}

static void expr_destroy_args(struct expr *e) {
  int i;
  struct expr arg;
  if (e->type == OP_FUNC) {
    vec_foreach(&e->param.func.args, arg, i) { expr_destroy_args(&arg); }
    vec_free(&e->param.func.args);
    if (e->param.func.context != NULL) {
      if (e->param.func.f->cleanup != NULL) {
        e->param.func.f->cleanup(e->param.func.f, e->param.func.context);
      }
      free(e->param.func.context);
    }
  } else if (e->type == OP_STRING) {
    free(e->param.str.s);
  } else if (e->type != OP_CONST && e->type != OP_VAR) {
    vec_foreach(&e->param.op.args, arg, i) { expr_destroy_args(&arg); }
    vec_free(&e->param.op.args);
  }
}

static void expr_destroy(struct expr *e, struct expr_var_list *vars) {
  if (e != NULL) {
    expr_destroy_args(e);
    free(e);
  }
  if (vars != NULL) {
    for (struct expr_var *v = vars->head; v;) {
      struct expr_var *next = v->next;
      free(v);
      v = next;
    }
  }
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif /* EXPR_H */
