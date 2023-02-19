#include "rc_internal.h"

#include "rc_compat.h"

#include <string.h>
#include <stdio.h>

int rc_parse_format(const char* format_str) {
  switch (*format_str++) {
    case 'F':
      if (!strcmp(format_str, "RAMES")) {
        return RC_FORMAT_FRAMES;
      }
      if (!strncmp(format_str, "LOAT", 4) && format_str[4] >= '1' && format_str[4] <= '6' && format_str[5] == '\0') {
        return RC_FORMAT_FLOAT1 + (format_str[4] - '1');
      }

      break;

    case 'T':
      if (!strcmp(format_str, "IME")) {
        return RC_FORMAT_FRAMES;
      }
      if (!strcmp(format_str, "IMESECS")) {
        return RC_FORMAT_SECONDS;
      }

      break;

    case 'S':
      if (!strcmp(format_str, "ECS")) {
        return RC_FORMAT_SECONDS;
      }
      if (!strcmp(format_str, "CORE")) {
        return RC_FORMAT_SCORE;
      }
      if (!strcmp(format_str, "ECS_AS_MINS")) {
        return RC_FORMAT_SECONDS_AS_MINUTES;
      }

      break;

    case 'M':
      if (!strcmp(format_str, "ILLISECS")) {
        return RC_FORMAT_CENTISECS;
      }
      if (!strcmp(format_str, "INUTES")) {
        return RC_FORMAT_MINUTES;
      }

      break;

    case 'P':
      if (!strcmp(format_str, "OINTS")) {
        return RC_FORMAT_SCORE;
      }

      break;

    case 'V':
      if (!strcmp(format_str, "ALUE")) {
        return RC_FORMAT_VALUE;
      }

      break;

    case 'O':
      if (!strcmp(format_str, "THER")) {
        return RC_FORMAT_SCORE;
      }

      break;
  }

  return RC_FORMAT_VALUE;
}

static int rc_format_value_minutes(char* buffer, int size, unsigned minutes) {
    unsigned hours;

    hours = minutes / 60;
    minutes -= hours * 60;
    return snprintf(buffer, size, "%uh%02u", hours, minutes);
}

static int rc_format_value_seconds(char* buffer, int size, unsigned seconds) {
  unsigned hours, minutes;

  /* apply modulus math to split the seconds into hours/minutes/seconds */
  minutes = seconds / 60;
  seconds -= minutes * 60;
  if (minutes < 60) {
    return snprintf(buffer, size, "%u:%02u", minutes, seconds);
  }

  hours = minutes / 60;
  minutes -= hours * 60;
  return snprintf(buffer, size, "%uh%02u:%02u", hours, minutes, seconds);
}

static int rc_format_value_centiseconds(char* buffer, int size, unsigned centiseconds) {
  unsigned seconds;
  int chars, chars2;

  /* modulus off the centiseconds */
  seconds = centiseconds / 100;
  centiseconds -= seconds * 100;

  chars = rc_format_value_seconds(buffer, size, seconds);
  if (chars > 0) {
    chars2 = snprintf(buffer + chars, size - chars, ".%02u", centiseconds);
    if (chars2 > 0) {
      chars += chars2;
    } else {
      chars = chars2;
    }
  }

  return chars;
}

int rc_format_typed_value(char* buffer, int size, const rc_typed_value_t* value, int format) {
  int chars;
  rc_typed_value_t converted_value;

  memcpy(&converted_value, value, sizeof(converted_value));

  switch (format) {
    default:
    case RC_FORMAT_VALUE:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      chars = snprintf(buffer, size, "%d", converted_value.value.i32);
      break;

    case RC_FORMAT_FRAMES:
      /* 60 frames per second = 100 centiseconds / 60 frames; multiply frames by 100 / 60 */
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_centiseconds(buffer, size, converted_value.value.u32 * 10 / 6);
      break;

    case RC_FORMAT_CENTISECS:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_centiseconds(buffer, size, converted_value.value.u32);
      break;

    case RC_FORMAT_SECONDS:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_seconds(buffer, size, converted_value.value.u32);
      break;

    case RC_FORMAT_SECONDS_AS_MINUTES:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_minutes(buffer, size, converted_value.value.u32 / 60);
      break;

    case RC_FORMAT_MINUTES:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_UNSIGNED);
      chars = rc_format_value_minutes(buffer, size, converted_value.value.u32);
      break;

    case RC_FORMAT_SCORE:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_SIGNED);
      chars = snprintf(buffer, size, "%06d", converted_value.value.i32);
      break;

    case RC_FORMAT_FLOAT1:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.1f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT2:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.2f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT3:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.3f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT4:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.4f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT5:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.5f", converted_value.value.f32);
      break;

    case RC_FORMAT_FLOAT6:
      rc_typed_value_convert(&converted_value, RC_VALUE_TYPE_FLOAT);
      chars = snprintf(buffer, size, "%.6f", converted_value.value.f32);
      break;
  }

  return chars;
}

int rc_format_value(char* buffer, int size, int value, int format) {
  rc_typed_value_t typed_value;

  typed_value.value.i32 = value;
  typed_value.type = RC_VALUE_TYPE_SIGNED;
  return rc_format_typed_value(buffer, size, &typed_value, format);
}
