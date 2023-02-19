#include "rc_api_common.h"
#include "rc_api_request.h"
#include "rc_api_runtime.h"

#include "../rcheevos/rc_compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETROACHIEVEMENTS_HOST "https://retroachievements.org"
#define RETROACHIEVEMENTS_IMAGE_HOST "http://i.retroachievements.org"
static char* g_host = NULL;
static char* g_imagehost = NULL;

#undef DEBUG_BUFFERS

/* --- rc_json --- */

static int rc_json_parse_object(const char** json_ptr, rc_json_field_t* fields, size_t field_count, unsigned* fields_seen);
static int rc_json_parse_array(const char** json_ptr, rc_json_field_t* field);

static int rc_json_parse_field(const char** json_ptr, rc_json_field_t* field) {
  int result;

  field->value_start = *json_ptr;

  switch (**json_ptr)
  {
    case '"': /* quoted string */
      ++(*json_ptr);
      while (**json_ptr != '"') {
        if (**json_ptr == '\\')
          ++(*json_ptr);

        if (**json_ptr == '\0')
          return RC_INVALID_JSON;

        ++(*json_ptr);
      }
      ++(*json_ptr);
      break;

    case '-':
    case '+': /* signed number */
      ++(*json_ptr);
      /* fallthrough to number */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': /* number */
      do {
        ++(*json_ptr);
      } while (**json_ptr >= '0' && **json_ptr <= '9');
      if (**json_ptr == '.') {
        do {
          ++(*json_ptr);
        } while (**json_ptr >= '0' && **json_ptr <= '9');
      }
      break;

    case '[': /* array */
      result = rc_json_parse_array(json_ptr, field);
      if (result != RC_OK)
          return result;

      break;

    case '{': /* object */
      result = rc_json_parse_object(json_ptr, NULL, 0, &field->array_size);
      if (result != RC_OK)
        return result;

      break;

    default: /* non-quoted text [true,false,null] */
      if (!isalpha((unsigned char)**json_ptr))
        return RC_INVALID_JSON;

      do {
        ++(*json_ptr);
      } while (isalnum((unsigned char)**json_ptr));
      break;
  }

  field->value_end = *json_ptr;
  return RC_OK;
}

static int rc_json_parse_array(const char** json_ptr, rc_json_field_t* field) {
  rc_json_field_t unused_field;
  const char* json = *json_ptr;
  int result;

  if (*json != '[')
    return RC_INVALID_JSON;
  ++json;

  field->array_size = 0;
  if (*json != ']') {
    do
    {
      while (isspace((unsigned char)*json))
        ++json;

      result = rc_json_parse_field(&json, &unused_field);
      if (result != RC_OK)
        return result;

      ++field->array_size;

      while (isspace((unsigned char)*json))
        ++json;

      if (*json != ',')
        break;

      ++json;
    } while (1);

    if (*json != ']')
      return RC_INVALID_JSON;
  }

  *json_ptr = ++json;
  return RC_OK;
}

static int rc_json_get_next_field(rc_json_object_field_iterator_t* iterator) {
  const char* json = iterator->json;

  while (isspace((unsigned char)*json))
    ++json;

  if (*json != '"')
    return RC_INVALID_JSON;

  iterator->field.name = ++json;
  while (*json != '"') {
    if (!*json)
      return RC_INVALID_JSON;
    ++json;
  }
  iterator->name_len = json - iterator->field.name;
  ++json;

  while (isspace((unsigned char)*json))
    ++json;

  if (*json != ':')
    return RC_INVALID_JSON;

  ++json;

  while (isspace((unsigned char)*json))
    ++json;

  if (rc_json_parse_field(&json, &iterator->field) < 0)
    return RC_INVALID_JSON;

  while (isspace((unsigned char)*json))
    ++json;

  iterator->json = json;
  return RC_OK;
}

static int rc_json_parse_object(const char** json_ptr, rc_json_field_t* fields, size_t field_count, unsigned* fields_seen) {
  rc_json_object_field_iterator_t iterator;
  const char* json = *json_ptr;
  size_t i;
  unsigned num_fields = 0;
  int result;

  if (fields_seen)
    *fields_seen = 0;

  for (i = 0; i < field_count; ++i)
    fields[i].value_start = fields[i].value_end = NULL;

  if (*json != '{')
    return RC_INVALID_JSON;
  ++json;

  if (*json == '}') {
    *json_ptr = ++json;
    return RC_OK;
  }

  memset(&iterator, 0, sizeof(iterator));
  iterator.json = json;

  do
  {
    result = rc_json_get_next_field(&iterator);
    if (result != RC_OK)
      return result;

    for (i = 0; i < field_count; ++i) {
      if (!fields[i].value_start && strncmp(fields[i].name, iterator.field.name, iterator.name_len) == 0 &&
          fields[i].name[iterator.name_len] == '\0') {
        fields[i].value_start = iterator.field.value_start;
        fields[i].value_end = iterator.field.value_end;
        fields[i].array_size = iterator.field.array_size;
        break;
      }
    }

    ++num_fields;
    if (*iterator.json != ',')
      break;

    ++iterator.json;
  } while (1);

  if (*iterator.json != '}')
    return RC_INVALID_JSON;

  if (fields_seen)
    *fields_seen = num_fields;

  *json_ptr = ++iterator.json;
  return RC_OK;
}

int rc_json_get_next_object_field(rc_json_object_field_iterator_t* iterator) {
  if (*iterator->json != ',' && *iterator->json != '{')
    return 0;

  ++iterator->json;
  return (rc_json_get_next_field(iterator) == RC_OK);
}

int rc_json_parse_response(rc_api_response_t* response, const char* json, rc_json_field_t* fields, size_t field_count) {
#ifndef NDEBUG
  if (field_count < 2)
    return RC_INVALID_STATE;
  if (strcmp(fields[0].name, "Success") != 0)
    return RC_INVALID_STATE;
  if (strcmp(fields[1].name, "Error") != 0)
    return RC_INVALID_STATE;
#endif

  if (*json == '{') {
    int result = rc_json_parse_object(&json, fields, field_count, NULL);

    rc_json_get_optional_string(&response->error_message, response, &fields[1], "Error", NULL);
    rc_json_get_optional_bool(&response->succeeded, &fields[0], "Success", 1);

    return result;
  }

  response->error_message = NULL;

  if (*json) {
    const char* end = json;
    while (*end && *end != '\n' && end - json < 200)
      ++end;

    if (end > json && end[-1] == '\r')
      --end;

    if (end > json) {
      char* dst = rc_buf_reserve(&response->buffer, (end - json) + 1);
      response->error_message = dst;
      memcpy(dst, json, end - json);
      dst += (end - json);
      *dst++ = '\0';
      rc_buf_consume(&response->buffer, response->error_message, dst);
    }
  }

  response->succeeded = 0;
  return RC_INVALID_JSON;
}

static int rc_json_missing_field(rc_api_response_t* response, const rc_json_field_t* field) {
  const char* not_found = " not found in response";
  const size_t not_found_len = strlen(not_found);
  const size_t field_len = strlen(field->name);

  char* write = rc_buf_reserve(&response->buffer, field_len + not_found_len + 1);
  if (write) {
    response->error_message = write;
    memcpy(write, field->name, field_len);
    write += field_len;
    memcpy(write, not_found, not_found_len + 1);
    write += not_found_len + 1;
    rc_buf_consume(&response->buffer, response->error_message, write);
  }

  response->succeeded = 0;
  return 0;
}

int rc_json_get_required_object(rc_json_field_t* fields, size_t field_count, rc_api_response_t* response, rc_json_field_t* field, const char* field_name) {
  const char* json = field->value_start;
#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (!json)
    return rc_json_missing_field(response, field);

  return (rc_json_parse_object(&json, fields, field_count, &field->array_size) == RC_OK);
}

static int rc_json_get_array_entry_value(rc_json_field_t* field, rc_json_field_t* iterator) {
  if (!iterator->array_size)
    return 0;

  while (isspace((unsigned char)*iterator->value_start))
    ++iterator->value_start;

  rc_json_parse_field(&iterator->value_start, field);

  while (isspace((unsigned char)*iterator->value_start))
    ++iterator->value_start;

  ++iterator->value_start; /* skip , or ] */

  --iterator->array_size;
  return 1;
}

int rc_json_get_required_unum_array(unsigned** entries, unsigned* num_entries, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  rc_json_field_t iterator;
  rc_json_field_t value;
  unsigned* entry;

  if (!rc_json_get_required_array(num_entries, &iterator, response, field, field_name))
    return RC_MISSING_VALUE;

  if (*num_entries) {
    *entries = (unsigned*)rc_buf_alloc(&response->buffer, *num_entries * sizeof(unsigned));
    if (!*entries)
      return RC_OUT_OF_MEMORY;

    value.name = field_name;

    entry = *entries;
    while (rc_json_get_array_entry_value(&value, &iterator)) {
      if (!rc_json_get_unum(entry, &value, field_name))
        return RC_MISSING_VALUE;

      ++entry;
    }
  }
  else {
    *entries = NULL;
  }

  return RC_OK;
}

int rc_json_get_required_array(unsigned* num_entries, rc_json_field_t* iterator, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (!field->value_start || *field->value_start != '[') {
    *num_entries = 0;
    return rc_json_missing_field(response, field);
  }

  memcpy(iterator, field, sizeof(*iterator));
  ++iterator->value_start; /* skip [ */

  *num_entries = field->array_size;
  return 1;
}

int rc_json_get_array_entry_object(rc_json_field_t* fields, size_t field_count, rc_json_field_t* iterator) {
  if (!iterator->array_size)
    return 0;

  while (isspace((unsigned char)*iterator->value_start))
    ++iterator->value_start;

  rc_json_parse_object(&iterator->value_start, fields, field_count, NULL);

  while (isspace((unsigned char)*iterator->value_start))
    ++iterator->value_start;

  ++iterator->value_start; /* skip , or ] */

  --iterator->array_size;
  return 1;
}

static unsigned rc_json_decode_hex4(const char* input) {
  char hex[5];

  memcpy(hex, input, 4);
  hex[4] = '\0';

  return (unsigned)strtoul(hex, NULL, 16);
}

static int rc_json_ucs32_to_utf8(unsigned char* dst, unsigned ucs32_char) {
  if (ucs32_char < 0x80) {
    dst[0] = (ucs32_char & 0x7F);
    return 1;
  }

  if (ucs32_char < 0x0800) {
    dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[0] = 0xC0 | (ucs32_char & 0x1F);
    return 2;
  }

  if (ucs32_char < 0x010000) {
    dst[2] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[0] = 0xE0 | (ucs32_char & 0x0F);
    return 3;
  }

  if (ucs32_char < 0x200000) {
    dst[3] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[2] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[0] = 0xF0 | (ucs32_char & 0x07);
    return 4;
  }

  if (ucs32_char < 0x04000000) {
    dst[4] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[3] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[2] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
    dst[0] = 0xF8 | (ucs32_char & 0x03);
    return 5;
  }

  dst[5] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[4] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[3] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[2] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[1] = 0x80 | (ucs32_char & 0x3F); ucs32_char >>= 6;
  dst[0] = 0xFC | (ucs32_char & 0x01);
  return 6;
}

int rc_json_get_string(const char** out, rc_api_buffer_t* buffer, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;
  size_t len = field->value_end - field->value_start;
  char* dst;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (!src) {
    *out = NULL;
    return 0;
  }

  if (len == 4 && memcmp(field->value_start, "null", 4) == 0) {
    *out = NULL;
    return 1;
  }

  if (*src == '\"') {
    ++src;

    if (*src == '\"') {
      /* simple optimization for empty string - don't allocate space */
      *out = "";
      return 1;
    }

    *out = dst = rc_buf_reserve(buffer, len - 1); /* -2 for quotes, +1 for null terminator */

    do {
      if (*src == '\\') {
        ++src;
        if (*src == 'n') {
          /* newline */
          ++src;
          *dst++ = '\n';
          continue;
        }

        if (*src == 'r') {
          /* carriage return */
          ++src;
          *dst++ = '\r';
          continue;
        }

        if (*src == 'u') {
          /* unicode character */
          unsigned ucs32_char = rc_json_decode_hex4(src + 1);
          src += 5;

          if (ucs32_char >= 0xD800 && ucs32_char < 0xE000) {
            /* surrogate lead - look for surrogate tail */
            if (ucs32_char < 0xDC00 && src[0] == '\\' && src[1] == 'u') {
              const unsigned surrogate = rc_json_decode_hex4(src + 2);
              src += 6;

              if (surrogate >= 0xDC00 && surrogate < 0xE000) {
                /* found a surrogate tail, merge them */
                ucs32_char = (((ucs32_char - 0xD800) << 10) | (surrogate - 0xDC00)) + 0x10000;
              }
            }

            if (!(ucs32_char & 0xFFFF0000)) {
              /* invalid surrogate pair, fallback to replacement char */
              ucs32_char = 0xFFFD;
            }
          }

          dst += rc_json_ucs32_to_utf8((unsigned char*)dst, ucs32_char);
          continue;
        }

        if (*src == 't') {
          /* tab */
          ++src;
          *dst++ = '\t';
          continue;
        }

        /* just an escaped character, fallthrough to normal copy */
      }

      *dst++ = *src++;
    } while (*src != '\"');

  } else {
    *out = dst = rc_buf_reserve(buffer, len + 1); /* +1 for null terminator */
    memcpy(dst, src, len);
    dst += len;
  }

  *dst++ = '\0';
  rc_buf_consume(buffer, *out, dst);
  return 1;
}

void rc_json_get_optional_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name, const char* default_value) {
  if (!rc_json_get_string(out, &response->buffer, field, field_name))
    *out = default_value;
}

int rc_json_get_required_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_string(out, &response->buffer, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_num(int* out, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;
  int value = 0;
  int negative = 0;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (!src) {
    *out = 0;
    return 0;
  }

  /* assert: string contains only numerals and an optional sign per rc_json_parse_field */
  if (*src == '-') {
    negative = 1;
    ++src;
  } else if (*src == '+') {
    ++src;
  } else if (*src < '0' || *src > '9') {
    *out = 0;
    return 0;
  }

  while (src < field->value_end && *src != '.') {
    value *= 10;
    value += *src - '0';
    ++src;
  }

  if (negative)
    *out = -value;
  else
    *out = value;

  return 1;
}

void rc_json_get_optional_num(int* out, const rc_json_field_t* field, const char* field_name, int default_value) {
  if (!rc_json_get_num(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_num(int* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_num(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_unum(unsigned* out, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;
  int value = 0;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (!src) {
    *out = 0;
    return 0;
  }

  if (*src < '0' || *src > '9') {
    *out = 0;
    return 0;
  }

  /* assert: string contains only numerals per rc_json_parse_field */
  while (src < field->value_end && *src != '.') {
    value *= 10;
    value += *src - '0';
    ++src;
  }

  *out = value;
  return 1;
}

void rc_json_get_optional_unum(unsigned* out, const rc_json_field_t* field, const char* field_name, unsigned default_value) {
  if (!rc_json_get_unum(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_unum(unsigned* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_unum(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_datetime(time_t* out, const rc_json_field_t* field, const char* field_name) {
  struct tm tm;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (*field->value_start == '\"') {
    memset(&tm, 0, sizeof(tm));
    if (sscanf(field->value_start + 1, "%d-%d-%d %d:%d:%d",
        &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec) == 6) {
      tm.tm_mon--; /* 0-based */
      tm.tm_year -= 1900; /* 1900 based */

      /* mktime converts a struct tm to a time_t using the local timezone.
       * the input string is UTC. since timegm is not universally cross-platform,
       * figure out the offset between UTC and local time by applying the
       * timezone conversion twice and manually removing the difference */
      {
         time_t local_timet = mktime(&tm);
         struct tm* gmt_tm = gmtime(&local_timet);
         time_t skewed_timet = mktime(gmt_tm); /* applies local time adjustment second time */
         time_t tz_offset = skewed_timet - local_timet;
         *out = local_timet - tz_offset;
      }

      return 1;
    }
  }

  *out = 0;
  return 0;
}

int rc_json_get_required_datetime(time_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_datetime(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_bool(int* out, const rc_json_field_t* field, const char* field_name) {
  const char* src = field->value_start;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (src) {
    const size_t len = field->value_end - field->value_start;
    if (len == 4 && strncasecmp(src, "true", 4) == 0) {
      *out = 1;
      return 1;
    } else if (len == 5 && strncasecmp(src, "false", 5) == 0) {
      *out = 0;
      return 1;
    } else if (len == 1) {
      *out = (*src != '0');
      return 1;
    }
  }

  *out = 0;
  return 0;
}

void rc_json_get_optional_bool(int* out, const rc_json_field_t* field, const char* field_name, int default_value) {
  if (!rc_json_get_bool(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_bool(int* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name) {
  if (rc_json_get_bool(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

/* --- rc_buf --- */

void rc_buf_init(rc_api_buffer_t* buffer) {
  buffer->write = &buffer->data[0];
  buffer->end = &buffer->data[sizeof(buffer->data)];
  buffer->next = NULL;
}

void rc_buf_destroy(rc_api_buffer_t* buffer) {
#ifdef DEBUG_BUFFERS
  int count = 0;
  int wasted = 0;
  int total = 0;
#endif

  /* first buffer is not allocated */
  buffer = buffer->next;

  /* deallocate any additional buffers */
  while (buffer) {
    rc_api_buffer_t* next = buffer->next;
#ifdef DEBUG_BUFFERS
    total += (int)(buffer->end - buffer->data);
    wasted += (int)(buffer->end - buffer->write);
    ++count;
#endif
    free(buffer);
    buffer = next;
  }

#ifdef DEBUG_BUFFERS
  printf("-- %d allocated buffers (%d/%d used, %d wasted, %0.2f%% efficiency)\n", count,
         total - wasted, total, wasted, (float)(100.0 - (wasted * 100.0) / total));
#endif
}

char* rc_buf_reserve(rc_api_buffer_t* buffer, size_t amount) {
  size_t remaining;
  while (buffer) {
    remaining = buffer->end - buffer->write;
    if (remaining >= amount)
      return buffer->write;

    if (!buffer->next) {
      /* allocate a chunk of memory that is a multiple of 256-bytes. casting it to an rc_api_buffer_t will
       * effectively unbound the data field, so use write and end pointers to track how data is being used.
       */
      const size_t buffer_prefix_size = sizeof(rc_api_buffer_t) - sizeof(buffer->data);
      const size_t alloc_size = (amount + buffer_prefix_size + 0xFF) & ~0xFF;
      buffer->next = (rc_api_buffer_t*)malloc(alloc_size);
      if (!buffer->next)
        break;

      buffer->next->write = buffer->next->data;
      buffer->next->end = buffer->next->write + (alloc_size - buffer_prefix_size);
      buffer->next->next = NULL;
    }

    buffer = buffer->next;
  }

  return NULL;
}

void rc_buf_consume(rc_api_buffer_t* buffer, const char* start, char* end) {
  do {
    if (buffer->write == start) {
      size_t offset = (end - buffer->data);
      offset = (offset + 7) & ~7;
      buffer->write = &buffer->data[offset];

      if (buffer->write > buffer->end)
        buffer->write = buffer->end;
      break;
    }

    buffer = buffer->next;
  } while (buffer);
}

void* rc_buf_alloc(rc_api_buffer_t* buffer, size_t amount) {
  char* ptr = rc_buf_reserve(buffer, amount);
  rc_buf_consume(buffer, ptr, ptr + amount);
  return (void*)ptr;
}

void rc_api_destroy_request(rc_api_request_t* request) {
  rc_buf_destroy(&request->buffer);
}

void rc_api_format_md5(char checksum[33], const unsigned char digest[16]) {
  snprintf(checksum, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
      digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
      digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
  );
}

/* --- rc_url_builder --- */

void rc_url_builder_init(rc_api_url_builder_t* builder, rc_api_buffer_t* buffer, size_t estimated_size) {
  rc_api_buffer_t* used_buffer;

  memset(builder, 0, sizeof(*builder));
  builder->buffer = buffer;
  builder->write = builder->start = rc_buf_reserve(buffer, estimated_size);

  used_buffer = buffer;
  while (used_buffer && used_buffer->write != builder->write)
    used_buffer = used_buffer->next;

  builder->end = (used_buffer) ? used_buffer->end : builder->start + estimated_size;
}

const char* rc_url_builder_finalize(rc_api_url_builder_t* builder) {
  rc_url_builder_append(builder, "", 1);

  if (builder->result != RC_OK)
    return NULL;

  rc_buf_consume(builder->buffer, builder->start, builder->write);
  return builder->start;
}

static int rc_url_builder_reserve(rc_api_url_builder_t* builder, size_t amount) {
  if (builder->result == RC_OK) {
    size_t remaining = builder->end - builder->write;
    if (remaining < amount) {
      const size_t used = builder->write - builder->start;
      const size_t current_size = builder->end - builder->start;
      const size_t buffer_prefix_size = sizeof(rc_api_buffer_t) - sizeof(builder->buffer->data);
      char* new_start;
      size_t new_size = (current_size < 256) ? 256 : current_size * 2;
      do {
        remaining = new_size - used;
        if (remaining >= amount)
          break;

        new_size *= 2;
      } while (1);

      /* rc_buf_reserve will align to 256 bytes after including the buffer prefix. attempt to account for that */
      if ((remaining - amount) > buffer_prefix_size)
        new_size -= buffer_prefix_size;

      new_start = rc_buf_reserve(builder->buffer, new_size);
      if (!new_start) {
        builder->result = RC_OUT_OF_MEMORY;
        return RC_OUT_OF_MEMORY;
      }

      if (new_start != builder->start) {
        memcpy(new_start, builder->start, used);
        builder->start = new_start;
        builder->write = new_start + used;
      }

      builder->end = builder->start + new_size;
    }
  }

  return builder->result;
}

void rc_url_builder_append_encoded_str(rc_api_url_builder_t* builder, const char* str) {
  static const char hex[] = "0123456789abcdef";
  const char* start = str;
  size_t len = 0;
  for (;;) {
    const char c = *str++;
    switch (c) {
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      case '-': case '_': case '.': case '~':
        len++;
        continue;

      case '\0':
        if (len)
          rc_url_builder_append(builder, start, len);

        return;

      default:
        if (rc_url_builder_reserve(builder, len + 3) != RC_OK)
          return;

        if (len) {
          memcpy(builder->write, start, len);
          builder->write += len;
        }

        if (c == ' ') {
          *builder->write++ = '+';
        } else {
          *builder->write++ = '%';
          *builder->write++ = hex[((unsigned char)c) >> 4];
          *builder->write++ = hex[c & 0x0F];
        }
        break;
    }

    start = str;
    len = 0;
  }
}

void rc_url_builder_append(rc_api_url_builder_t* builder, const char* data, size_t len) {
  if (rc_url_builder_reserve(builder, len) == RC_OK) {
    memcpy(builder->write, data, len);
    builder->write += len;
  }
}

static int rc_url_builder_append_param_equals(rc_api_url_builder_t* builder, const char* param) {
  size_t param_len = strlen(param);

  if (rc_url_builder_reserve(builder, param_len + 2) == RC_OK) {
    if (builder->write > builder->start) {
      if (builder->write[-1] != '?')
        *builder->write++ = '&';
    }

    memcpy(builder->write, param, param_len);
    builder->write += param_len;
    *builder->write++ = '=';
  }

  return builder->result;
}

void rc_url_builder_append_unum_param(rc_api_url_builder_t* builder, const char* param, unsigned value) {
  if (rc_url_builder_append_param_equals(builder, param) == RC_OK) {
    char num[16];
    int chars = snprintf(num, sizeof(num), "%u", value);
    rc_url_builder_append(builder, num, chars);
  }
}

void rc_url_builder_append_num_param(rc_api_url_builder_t* builder, const char* param, int value) {
  if (rc_url_builder_append_param_equals(builder, param) == RC_OK) {
    char num[16];
    int chars = snprintf(num, sizeof(num), "%d", value);
    rc_url_builder_append(builder, num, chars);
  }
}

void rc_url_builder_append_str_param(rc_api_url_builder_t* builder, const char* param, const char* value) {
  rc_url_builder_append_param_equals(builder, param);
  rc_url_builder_append_encoded_str(builder, value);
}

void rc_api_url_build_dorequest_url(rc_api_request_t* request) {
  #define DOREQUEST_ENDPOINT "/dorequest.php"
  rc_buf_init(&request->buffer);

  if (!g_host) {
    request->url = RETROACHIEVEMENTS_HOST DOREQUEST_ENDPOINT;
  }
  else {
    const size_t endpoint_len = sizeof(DOREQUEST_ENDPOINT);
    const size_t host_len = strlen(g_host);
    const size_t url_len = host_len + endpoint_len;
    char* url = rc_buf_reserve(&request->buffer, url_len);

    memcpy(url, g_host, host_len);
    memcpy(url + host_len, DOREQUEST_ENDPOINT, endpoint_len);
    rc_buf_consume(&request->buffer, url, url + url_len);

    request->url = url;
  }
  #undef DOREQUEST_ENDPOINT
}

int rc_api_url_build_dorequest(rc_api_url_builder_t* builder, const char* api, const char* username, const char* api_token) {
  if (!username || !*username || !api_token || !*api_token) {
    builder->result = RC_INVALID_STATE;
    return 0;
  }

  rc_url_builder_append_str_param(builder, "r", api);
  rc_url_builder_append_str_param(builder, "u", username);
  rc_url_builder_append_str_param(builder, "t", api_token);

  return (builder->result == RC_OK);
}

/* --- Set Host --- */

static void rc_api_update_host(char** host, const char* hostname) {
  if (*host != NULL)
    free(*host);

  if (hostname != NULL) {
    if (strstr(hostname, "://")) {
      *host = strdup(hostname);
    }
    else {
      const size_t hostname_len = strlen(hostname);
      if (hostname_len == 0) {
        *host = NULL;
      }
      else {
        char* newhost = (char*)malloc(hostname_len + 7 + 1);
        if (newhost) {
          memcpy(newhost, "http://", 7);
          memcpy(&newhost[7], hostname, hostname_len + 1);
          *host = newhost;
        }
        else {
          *host = NULL;
        }
      }
    }
  }
  else {
    *host = NULL;
  }
}

void rc_api_set_host(const char* hostname) {
  rc_api_update_host(&g_host, hostname);
}

void rc_api_set_image_host(const char* hostname) {
  rc_api_update_host(&g_imagehost, hostname);
}

/* --- Fetch Image --- */

int rc_api_init_fetch_image_request(rc_api_request_t* request, const rc_api_fetch_image_request_t* api_params) {
  rc_api_url_builder_t builder;

  rc_buf_init(&request->buffer);
  rc_url_builder_init(&builder, &request->buffer, 64);

  if (g_imagehost) {
    rc_url_builder_append(&builder, g_imagehost, strlen(g_imagehost));
  }
  else if (g_host) {
    rc_url_builder_append(&builder, g_host, strlen(g_host));
  }
  else {
    rc_url_builder_append(&builder, RETROACHIEVEMENTS_IMAGE_HOST, sizeof(RETROACHIEVEMENTS_IMAGE_HOST) - 1);
  }

  switch (api_params->image_type)
  {
    case RC_IMAGE_TYPE_GAME:
      rc_url_builder_append(&builder, "/Images/", 8);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, ".png", 4);
      break;

    case RC_IMAGE_TYPE_ACHIEVEMENT:
      rc_url_builder_append(&builder, "/Badge/", 7);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, ".png", 4);
      break;

    case RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED:
      rc_url_builder_append(&builder, "/Badge/", 7);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, "_lock.png", 9);
      break;

    case RC_IMAGE_TYPE_USER:
      rc_url_builder_append(&builder, "/UserPic/", 9);
      rc_url_builder_append(&builder, api_params->image_name, strlen(api_params->image_name));
      rc_url_builder_append(&builder, ".png", 4);
      break;

    default:
      return RC_INVALID_STATE;
  }

  request->url = rc_url_builder_finalize(&builder);
  request->post_data = NULL;

  return builder.result;
}
