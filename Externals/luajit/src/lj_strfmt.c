/*
** String formatting.
** Copyright (C) 2005-2015 Mike Pall. See Copyright Notice in luajit.h
*/

#include <stdio.h>

#define lj_strfmt_c
#define LUA_CORE

#include "lj_obj.h"
#include "lj_buf.h"
#include "lj_str.h"
#include "lj_state.h"
#include "lj_char.h"
#include "lj_strfmt.h"

/* -- Format parser ------------------------------------------------------- */

static const uint8_t strfmt_map[('x'-'A')+1] = {
  STRFMT_A,0,0,0,STRFMT_E,0,STRFMT_G,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,STRFMT_X,0,0,
  0,0,0,0,0,0,
  STRFMT_A,0,STRFMT_C,STRFMT_D,STRFMT_E,STRFMT_F,STRFMT_G,0,STRFMT_I,0,0,0,0,
  0,STRFMT_O,STRFMT_P,STRFMT_Q,0,STRFMT_S,0,STRFMT_U,0,0,STRFMT_X
};

SFormat LJ_FASTCALL lj_strfmt_parse(FormatState *fs)
{
  const uint8_t *p = fs->p, *e = fs->e;
  fs->str = (const char *)p;
  for (; p < e; p++) {
    if (*p == '%') {  /* Escape char? */
      if (p[1] == '%') {  /* '%%'? */
	fs->p = ++p+1;
	goto retlit;
      } else {
	SFormat sf = 0;
	uint32_t c;
	if (p != (const uint8_t *)fs->str)
	  break;
	for (p++; (uint32_t)*p - ' ' <= (uint32_t)('0' - ' '); p++) {
	  /* Parse flags. */
	  if (*p == '-') sf |= STRFMT_F_LEFT;
	  else if (*p == '+') sf |= STRFMT_F_PLUS;
	  else if (*p == '0') sf |= STRFMT_F_ZERO;
	  else if (*p == ' ') sf |= STRFMT_F_SPACE;
	  else if (*p == '#') sf |= STRFMT_F_ALT;
	  else break;
	}
	if ((uint32_t)*p - '0' < 10) {  /* Parse width. */
	  uint32_t width = (uint32_t)*p++ - '0';
	  if ((uint32_t)*p - '0' < 10)
	    width = (uint32_t)*p++ - '0' + width*10;
	  sf |= (width << STRFMT_SH_WIDTH);
	}
	if (*p == '.') {  /* Parse precision. */
	  uint32_t prec = 0;
	  p++;
	  if ((uint32_t)*p - '0' < 10) {
	    prec = (uint32_t)*p++ - '0';
	    if ((uint32_t)*p - '0' < 10)
	      prec = (uint32_t)*p++ - '0' + prec*10;
	  }
	  sf |= ((prec+1) << STRFMT_SH_PREC);
	}
	/* Parse conversion. */
	c = (uint32_t)*p - 'A';
	if (LJ_LIKELY(c <= (uint32_t)('x' - 'A'))) {
	  uint32_t sx = strfmt_map[c];
	  if (sx) {
	    fs->p = p+1;
	    return (sf | sx | ((c & 0x20) ? 0 : STRFMT_F_UPPER));
	  }
	}
	/* Return error location. */
	if (*p >= 32) p++;
	fs->len = (MSize)(p - (const uint8_t *)fs->str);
	fs->p = fs->e;
	return STRFMT_ERR;
      }
    }
  }
  fs->p = p;
retlit:
  fs->len = (MSize)(p - (const uint8_t *)fs->str);
  return fs->len ? STRFMT_LIT : STRFMT_EOF;
}

/* -- Raw conversions ----------------------------------------------------- */

/* Write number to bufer. */
char * LJ_FASTCALL lj_strfmt_wnum(char *p, cTValue *o)
{
  if (LJ_LIKELY((o->u32.hi << 1) < 0xffe00000)) {  /* Finite? */
#if __BIONIC__
    if (tvismzero(o)) { *p++ = '-'; *p++ = '0'; return p; }
#endif
    return p + lua_number2str(p, o->n);
  } else if (((o->u32.hi & 0x000fffff) | o->u32.lo) != 0) {
    *p++ = 'n'; *p++ = 'a'; *p++ = 'n';
  } else if ((o->u32.hi & 0x80000000) == 0) {
    *p++ = 'i'; *p++ = 'n'; *p++ = 'f';
  } else {
    *p++ = '-'; *p++ = 'i'; *p++ = 'n'; *p++ = 'f';
  }
  return p;
}

#define WINT_R(x, sh, sc) \
  { uint32_t d = (x*(((1<<sh)+sc-1)/sc))>>sh; x -= d*sc; *p++ = (char)('0'+d); }

/* Write integer to buffer. */
char * LJ_FASTCALL lj_strfmt_wint(char *p, int32_t k)
{
  uint32_t u = (uint32_t)k;
  if (k < 0) { u = (uint32_t)-k; *p++ = '-'; }
  if (u < 10000) {
    if (u < 10) goto dig1; if (u < 100) goto dig2; if (u < 1000) goto dig3;
  } else {
    uint32_t v = u / 10000; u -= v * 10000;
    if (v < 10000) {
      if (v < 10) goto dig5; if (v < 100) goto dig6; if (v < 1000) goto dig7;
    } else {
      uint32_t w = v / 10000; v -= w * 10000;
      if (w >= 10) WINT_R(w, 10, 10)
      *p++ = (char)('0'+w);
    }
    WINT_R(v, 23, 1000)
    dig7: WINT_R(v, 12, 100)
    dig6: WINT_R(v, 10, 10)
    dig5: *p++ = (char)('0'+v);
  }
  WINT_R(u, 23, 1000)
  dig3: WINT_R(u, 12, 100)
  dig2: WINT_R(u, 10, 10)
  dig1: *p++ = (char)('0'+u);
  return p;
}
#undef WINT_R

/* Write pointer to buffer. */
char * LJ_FASTCALL lj_strfmt_wptr(char *p, const void *v)
{
  ptrdiff_t x = (ptrdiff_t)v;
  MSize i, n = STRFMT_MAXBUF_PTR;
  if (x == 0) {
    *p++ = 'N'; *p++ = 'U'; *p++ = 'L'; *p++ = 'L';
    return p;
  }
#if LJ_64
  /* Shorten output for 64 bit pointers. */
  n = 2+2*4+((x >> 32) ? 2+2*(lj_fls((uint32_t)(x >> 32))>>3) : 0);
#endif
  p[0] = '0';
  p[1] = 'x';
  for (i = n-1; i >= 2; i--, x >>= 4)
    p[i] = "0123456789abcdef"[(x & 15)];
  return p+n;
}

/* Write ULEB128 to buffer. */
char * LJ_FASTCALL lj_strfmt_wuleb128(char *p, uint32_t v)
{
  for (; v >= 0x80; v >>= 7)
    *p++ = (char)((v & 0x7f) | 0x80);
  *p++ = (char)v;
  return p;
}

/* Return string or write number to buffer and return pointer to start. */
const char *lj_strfmt_wstrnum(char *buf, cTValue *o, MSize *lenp)
{
  if (tvisstr(o)) {
    *lenp = strV(o)->len;
    return strVdata(o);
  } else if (tvisint(o)) {
    *lenp = (MSize)(lj_strfmt_wint(buf, intV(o)) - buf);
    return buf;
  } else if (tvisnum(o)) {
    *lenp = (MSize)(lj_strfmt_wnum(buf, o) - buf);
    return buf;
  } else {
    return NULL;
  }
}

/* -- Unformatted conversions to buffer ----------------------------------- */

/* Add integer to buffer. */
SBuf * LJ_FASTCALL lj_strfmt_putint(SBuf *sb, int32_t k)
{
  setsbufP(sb, lj_strfmt_wint(lj_buf_more(sb, STRFMT_MAXBUF_INT), k));
  return sb;
}

#if LJ_HASJIT
/* Add number to buffer. */
SBuf * LJ_FASTCALL lj_strfmt_putnum(SBuf *sb, cTValue *o)
{
  setsbufP(sb, lj_strfmt_wnum(lj_buf_more(sb, STRFMT_MAXBUF_NUM), o));
  return sb;
}
#endif

SBuf * LJ_FASTCALL lj_strfmt_putptr(SBuf *sb, const void *v)
{
  setsbufP(sb, lj_strfmt_wptr(lj_buf_more(sb, STRFMT_MAXBUF_PTR), v));
  return sb;
}

/* Add quoted string to buffer. */
SBuf * LJ_FASTCALL lj_strfmt_putquoted(SBuf *sb, GCstr *str)
{
  const char *s = strdata(str);
  MSize len = str->len;
  lj_buf_putb(sb, '"');
  while (len--) {
    uint32_t c = (uint32_t)(uint8_t)*s++;
    char *p = lj_buf_more(sb, 4);
    if (c == '"' || c == '\\' || c == '\n') {
      *p++ = '\\';
    } else if (lj_char_iscntrl(c)) {  /* This can only be 0-31 or 127. */
      uint32_t d;
      *p++ = '\\';
      if (c >= 100 || lj_char_isdigit((uint8_t)*s)) {
	*p++ = (char)('0'+(c >= 100)); if (c >= 100) c -= 100;
	goto tens;
      } else if (c >= 10) {
      tens:
	d = (c * 205) >> 11; c -= d * 10; *p++ = (char)('0'+d);
      }
      c += '0';
    }
    *p++ = (char)c;
    setsbufP(sb, p);
  }
  lj_buf_putb(sb, '"');
  return sb;
}

/* -- Formatted conversions to buffer ------------------------------------- */

/* Add formatted char to buffer. */
SBuf *lj_strfmt_putfchar(SBuf *sb, SFormat sf, int32_t c)
{
  MSize width = STRFMT_WIDTH(sf);
  char *p = lj_buf_more(sb, width > 1 ? width : 1);
  if ((sf & STRFMT_F_LEFT)) *p++ = (char)c;
  while (width-- > 1) *p++ = ' ';
  if (!(sf & STRFMT_F_LEFT)) *p++ = (char)c;
  setsbufP(sb, p);
  return sb;
}

/* Add formatted string to buffer. */
SBuf *lj_strfmt_putfstr(SBuf *sb, SFormat sf, GCstr *str)
{
  MSize len = str->len <= STRFMT_PREC(sf) ? str->len : STRFMT_PREC(sf);
  MSize width = STRFMT_WIDTH(sf);
  char *p = lj_buf_more(sb, width > len ? width : len);
  if ((sf & STRFMT_F_LEFT)) p = lj_buf_wmem(p, strdata(str), len);
  while (width-- > len) *p++ = ' ';
  if (!(sf & STRFMT_F_LEFT)) p = lj_buf_wmem(p, strdata(str), len);
  setsbufP(sb, p);
  return sb;
}

/* Add formatted signed/unsigned integer to buffer. */
SBuf *lj_strfmt_putfxint(SBuf *sb, SFormat sf, uint64_t k)
{
  char buf[STRFMT_MAXBUF_XINT], *q = buf + sizeof(buf), *p;
#ifdef LUA_USE_ASSERT
  char *ps;
#endif
  MSize prefix = 0, len, prec, pprec, width, need;

  /* Figure out signed prefixes. */
  if (STRFMT_TYPE(sf) == STRFMT_INT) {
    if ((int64_t)k < 0) {
      k = (uint64_t)-(int64_t)k;
      prefix = 256 + '-';
    } else if ((sf & STRFMT_F_PLUS)) {
      prefix = 256 + '+';
    } else if ((sf & STRFMT_F_SPACE)) {
      prefix = 256 + ' ';
    }
  }

  /* Convert number and store to fixed-size buffer in reverse order. */
  prec = STRFMT_PREC(sf);
  if ((int32_t)prec >= 0) sf &= ~STRFMT_F_ZERO;
  if (k == 0) {  /* Special-case zero argument. */
    if (prec != 0 ||
	(sf & (STRFMT_T_OCT|STRFMT_F_ALT)) == (STRFMT_T_OCT|STRFMT_F_ALT))
      *--q = '0';
  } else if (!(sf & (STRFMT_T_HEX|STRFMT_T_OCT))) {  /* Decimal. */
    uint32_t k2;
    while ((k >> 32)) { *--q = (char)('0' + k % 10); k /= 10; }
    k2 = (uint32_t)k;
    do { *--q = (char)('0' + k2 % 10); k2 /= 10; } while (k2);
  } else if ((sf & STRFMT_T_HEX)) {  /* Hex. */
    const char *hexdig = (sf & STRFMT_F_UPPER) ? "0123456789ABCDEF" :
						 "0123456789abcdef";
    do { *--q = hexdig[(k & 15)]; k >>= 4; } while (k);
    if ((sf & STRFMT_F_ALT)) prefix = 512 + ((sf & STRFMT_F_UPPER) ? 'X' : 'x');
  } else {  /* Octal. */
    do { *--q = (char)('0' + (uint32_t)(k & 7)); k >>= 3; } while (k);
    if ((sf & STRFMT_F_ALT)) *--q = '0';
  }

  /* Calculate sizes. */
  len = (MSize)(buf + sizeof(buf) - q);
  if ((int32_t)len >= (int32_t)prec) prec = len;
  width = STRFMT_WIDTH(sf);
  pprec = prec + (prefix >> 8);
  need = width > pprec ? width : pprec;
  p = lj_buf_more(sb, need);
#ifdef LUA_USE_ASSERT
  ps = p;
#endif

  /* Format number with leading/trailing whitespace and zeros. */
  if ((sf & (STRFMT_F_LEFT|STRFMT_F_ZERO)) == 0)
    while (width-- > pprec) *p++ = ' ';
  if (prefix) {
    if ((char)prefix >= 'X') *p++ = '0';
    *p++ = (char)prefix;
  }
  if ((sf & (STRFMT_F_LEFT|STRFMT_F_ZERO)) == STRFMT_F_ZERO)
    while (width-- > pprec) *p++ = '0';
  while (prec-- > len) *p++ = '0';
  while (q < buf + sizeof(buf)) *p++ = *q++;  /* Add number itself. */
  if ((sf & STRFMT_F_LEFT))
    while (width-- > pprec) *p++ = ' ';

  lua_assert(need == (MSize)(p - ps));
  setsbufP(sb, p);
  return sb;
}

/* Add number formatted as signed integer to buffer. */
SBuf *lj_strfmt_putfnum_int(SBuf *sb, SFormat sf, lua_Number n)
{
  int64_t k = (int64_t)n;
  if (checki32(k) && sf == STRFMT_INT)
    return lj_strfmt_putint(sb, (int32_t)k);  /* Shortcut for plain %d. */
  else
    return lj_strfmt_putfxint(sb, sf, (uint64_t)k);
}

/* Add number formatted as unsigned integer to buffer. */
SBuf *lj_strfmt_putfnum_uint(SBuf *sb, SFormat sf, lua_Number n)
{
  int64_t k;
  if (n >= 9223372036854775808.0)
    k = (int64_t)(n - 18446744073709551616.0);
  else
    k = (int64_t)n;
  return lj_strfmt_putfxint(sb, sf, (uint64_t)k);
}

/* Max. sprintf buffer size needed. At least #string.format("%.99f", -1e308). */
#define STRFMT_FMTNUMBUF	512

/* Add formatted floating-point number to buffer. */
SBuf *lj_strfmt_putfnum(SBuf *sb, SFormat sf, lua_Number n)
{
  TValue tv;
  tv.n = n;
  if (LJ_UNLIKELY((tv.u32.hi << 1) >= 0xffe00000)) {
    /* Canonicalize output of non-finite values. */
    MSize width = STRFMT_WIDTH(sf), len = 3;
    int prefix = 0, ch = (sf & STRFMT_F_UPPER) ? 0x202020 : 0;
    char *p;
    if (((tv.u32.hi & 0x000fffff) | tv.u32.lo) != 0) {
      ch ^= ('n' << 16) | ('a' << 8) | 'n';
      if ((sf & STRFMT_F_SPACE)) prefix = ' ';
    } else {
      ch ^= ('i' << 16) | ('n' << 8) | 'f';
      if ((tv.u32.hi & 0x80000000)) prefix = '-';
      else if ((sf & STRFMT_F_PLUS)) prefix = '+';
      else if ((sf & STRFMT_F_SPACE)) prefix = ' ';
    }
    if (prefix) len = 4;
    p = lj_buf_more(sb, width > len ? width : len);
    if (!(sf & STRFMT_F_LEFT)) while (width-- > len) *p++ = ' ';
    if (prefix) *p++ = prefix;
    *p++ = (char)(ch >> 16); *p++ = (char)(ch >> 8); *p++ = (char)ch;
    if ((sf & STRFMT_F_LEFT)) while (width-- > len) *p++ = ' ';
    setsbufP(sb, p);
  } else {  /* Delegate to sprintf() for now. */
    uint8_t width = (uint8_t)STRFMT_WIDTH(sf), prec = (uint8_t)STRFMT_PREC(sf);
    char fmt[1+5+2+3+1+1], *p = fmt;
    *p++ = '%';
    if ((sf & STRFMT_F_LEFT)) *p++ = '-';
    if ((sf & STRFMT_F_PLUS)) *p++ = '+';
    if ((sf & STRFMT_F_ZERO)) *p++ = '0';
    if ((sf & STRFMT_F_SPACE)) *p++ = ' ';
    if ((sf & STRFMT_F_ALT)) *p++ = '#';
    if (width) {
      uint8_t x = width / 10, y = width % 10;
      if (x) *p++ = '0' + x;
      *p++ = '0' + y;
    }
    if (prec != 255) {
      uint8_t x = prec / 10, y = prec % 10;
      *p++ = '.';
      if (x) *p++ = '0' + x;
      *p++ = '0' + y;
    }
    *p++ = (0x67666561 >> (STRFMT_FP(sf)<<3)) ^ ((sf & STRFMT_F_UPPER)?0x20:0);
    *p = '\0';
    p = lj_buf_more(sb, STRFMT_FMTNUMBUF);
    setsbufP(sb, p + sprintf(p, fmt, n));
  }
  return sb;
}

/* -- Conversions to strings ---------------------------------------------- */

/* Convert integer to string. */
GCstr * LJ_FASTCALL lj_strfmt_int(lua_State *L, int32_t k)
{
  char buf[STRFMT_MAXBUF_INT];
  MSize len = (MSize)(lj_strfmt_wint(buf, k) - buf);
  return lj_str_new(L, buf, len);
}

/* Convert number to string. */
GCstr * LJ_FASTCALL lj_strfmt_num(lua_State *L, cTValue *o)
{
  char buf[STRFMT_MAXBUF_NUM];
  MSize len = (MSize)(lj_strfmt_wnum(buf, o) - buf);
  return lj_str_new(L, buf, len);
}

/* Convert integer or number to string. */
GCstr * LJ_FASTCALL lj_strfmt_number(lua_State *L, cTValue *o)
{
  return tvisint(o) ? lj_strfmt_int(L, intV(o)) : lj_strfmt_num(L, o);
}

#if LJ_HASJIT
/* Convert char value to string. */
GCstr * LJ_FASTCALL lj_strfmt_char(lua_State *L, int c)
{
  char buf[1];
  buf[0] = c;
  return lj_str_new(L, buf, 1);
}
#endif

/* Raw conversion of object to string. */
GCstr * LJ_FASTCALL lj_strfmt_obj(lua_State *L, cTValue *o)
{
  if (tvisstr(o)) {
    return strV(o);
  } else if (tvisnumber(o)) {
    return lj_strfmt_number(L, o);
  } else if (tvisnil(o)) {
    return lj_str_newlit(L, "nil");
  } else if (tvisfalse(o)) {
    return lj_str_newlit(L, "false");
  } else if (tvistrue(o)) {
    return lj_str_newlit(L, "true");
  } else {
    char buf[8+2+2+16], *p = buf;
    p = lj_buf_wmem(p, lj_typename(o), (MSize)strlen(lj_typename(o)));
    *p++ = ':'; *p++ = ' ';
    if (tvisfunc(o) && isffunc(funcV(o))) {
      p = lj_buf_wmem(p, "builtin#", 8);
      p = lj_strfmt_wint(p, funcV(o)->c.ffid);
    } else {
      p = lj_strfmt_wptr(p, lj_obj_ptr(o));
    }
    return lj_str_new(L, buf, (size_t)(p - buf));
  }
}

/* -- Internal string formatting ------------------------------------------ */

/*
** These functions are only used for lua_pushfstring(), lua_pushvfstring()
** and for internal string formatting (e.g. error messages). Caveat: unlike
** string.format(), only a limited subset of formats and flags are supported!
**
** LuaJIT has support for a couple more formats than Lua 5.1/5.2:
** - %d %u %o %x with full formatting, 32 bit integers only.
** - %f and other FP formats are really %.14g.
** - %s %c %p without formatting.
*/

/* Push formatted message as a string object to Lua stack. va_list variant. */
const char *lj_strfmt_pushvf(lua_State *L, const char *fmt, va_list argp)
{
  SBuf *sb = lj_buf_tmp_(L);
  FormatState fs;
  SFormat sf;
  GCstr *str;
  lj_strfmt_init(&fs, fmt, (MSize)strlen(fmt));
  while ((sf = lj_strfmt_parse(&fs)) != STRFMT_EOF) {
    switch (STRFMT_TYPE(sf)) {
    case STRFMT_LIT:
      lj_buf_putmem(sb, fs.str, fs.len);
      break;
    case STRFMT_INT:
      lj_strfmt_putfxint(sb, sf, va_arg(argp, int32_t));
      break;
    case STRFMT_UINT:
      lj_strfmt_putfxint(sb, sf, va_arg(argp, uint32_t));
      break;
    case STRFMT_NUM: {
      TValue tv;
      tv.n = va_arg(argp, lua_Number);
      setsbufP(sb, lj_strfmt_wnum(lj_buf_more(sb, STRFMT_MAXBUF_NUM), &tv));
      break;
      }
    case STRFMT_STR: {
      const char *s = va_arg(argp, char *);
      if (s == NULL) s = "(null)";
      lj_buf_putmem(sb, s, (MSize)strlen(s));
      break;
      }
    case STRFMT_CHAR:
      lj_buf_putb(sb, va_arg(argp, int));
      break;
    case STRFMT_PTR:
      lj_strfmt_putptr(sb, va_arg(argp, void *));
      break;
    case STRFMT_ERR:
    default:
      lj_buf_putb(sb, '?');
      lua_assert(0);
      break;
    }
  }
  str = lj_buf_str(L, sb);
  setstrV(L, L->top, str);
  incr_top(L);
  return strdata(str);
}

/* Push formatted message as a string object to Lua stack. Vararg variant. */
const char *lj_strfmt_pushf(lua_State *L, const char *fmt, ...)
{
  const char *msg;
  va_list argp;
  va_start(argp, fmt);
  msg = lj_strfmt_pushvf(L, fmt, argp);
  va_end(argp);
  return msg;
}

