#ifndef RC_API_COMMON_H
#define RC_API_COMMON_H

#include "rc_api_request.h"

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rc_api_url_builder_t {
  char* write;
  char* start;
  char* end;
  rc_api_buffer_t* buffer;
  int result;
}
rc_api_url_builder_t;

void rc_url_builder_init(rc_api_url_builder_t* builder, rc_api_buffer_t* buffer, size_t estimated_size);
void rc_url_builder_append(rc_api_url_builder_t* builder, const char* data, size_t len);
const char* rc_url_builder_finalize(rc_api_url_builder_t* builder);

typedef struct rc_json_field_t {
  const char* name;
  const char* value_start;
  const char* value_end;
  unsigned array_size;
}
rc_json_field_t;

typedef struct rc_json_object_field_iterator_t {
  rc_json_field_t field;
  const char* json;
  size_t name_len;
}
rc_json_object_field_iterator_t;

int rc_json_parse_response(rc_api_response_t* response, const char* json, rc_json_field_t* fields, size_t field_count);
int rc_json_get_string(const char** out, rc_api_buffer_t* buffer, const rc_json_field_t* field, const char* field_name);
int rc_json_get_num(int* out, const rc_json_field_t* field, const char* field_name);
int rc_json_get_unum(unsigned* out, const rc_json_field_t* field, const char* field_name);
int rc_json_get_bool(int* out, const rc_json_field_t* field, const char* field_name);
int rc_json_get_datetime(time_t* out, const rc_json_field_t* field, const char* field_name);
void rc_json_get_optional_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name, const char* default_value);
void rc_json_get_optional_num(int* out, const rc_json_field_t* field, const char* field_name, int default_value);
void rc_json_get_optional_unum(unsigned* out, const rc_json_field_t* field, const char* field_name, unsigned default_value);
void rc_json_get_optional_bool(int* out, const rc_json_field_t* field, const char* field_name, int default_value);
int rc_json_get_required_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_num(int* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_unum(unsigned* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_bool(int* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_datetime(time_t* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_object(rc_json_field_t* fields, size_t field_count, rc_api_response_t* response, rc_json_field_t* field, const char* field_name);
int rc_json_get_required_unum_array(unsigned** entries, unsigned* num_entries, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_required_array(unsigned* num_entries, rc_json_field_t* iterator, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name);
int rc_json_get_array_entry_object(rc_json_field_t* fields, size_t field_count, rc_json_field_t* iterator);
int rc_json_get_next_object_field(rc_json_object_field_iterator_t* iterator);

void rc_buf_init(rc_api_buffer_t* buffer);
void rc_buf_destroy(rc_api_buffer_t* buffer);
char* rc_buf_reserve(rc_api_buffer_t* buffer, size_t amount);
void rc_buf_consume(rc_api_buffer_t* buffer, const char* start, char* end);
void* rc_buf_alloc(rc_api_buffer_t* buffer, size_t amount);

void rc_url_builder_append_encoded_str(rc_api_url_builder_t* builder, const char* str);
void rc_url_builder_append_num_param(rc_api_url_builder_t* builder, const char* param, int value);
void rc_url_builder_append_unum_param(rc_api_url_builder_t* builder, const char* param, unsigned value);
void rc_url_builder_append_str_param(rc_api_url_builder_t* builder, const char* param, const char* value);

void rc_api_url_build_dorequest_url(rc_api_request_t* request);
int rc_api_url_build_dorequest(rc_api_url_builder_t* builder, const char* api, const char* username, const char* api_token);
void rc_api_format_md5(char checksum[33], const unsigned char digest[16]);

#ifdef __cplusplus
}
#endif

#endif /* RC_API_COMMON_H */
