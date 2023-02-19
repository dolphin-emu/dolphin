#include "rc_internal.h"
#include "rc_url.h"
#include "rc_api_runtime.h"
#include "rc_consoles.h"
#include "rc_validate.h"

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h> /* memset */

#ifdef _CRT_SECURE_NO_WARNINGS /* windows build*/
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#else
 #include <dirent.h>
 #include <strings.h>
 #define stricmp strcasecmp
#endif

/* usage exmaple:
 *
 * ./validator.exe d "E:\RetroAchievements\dump" | sort > results.txt
 * grep -v ": OK" results.txt  | grep -v "File: " | grep .
 */

static const char* flag_string(char type) {
  switch (type) {
    case RC_CONDITION_STANDARD: return "";
    case RC_CONDITION_PAUSE_IF: return "PauseIf ";
    case RC_CONDITION_RESET_IF: return "ResetIf ";
    case RC_CONDITION_MEASURED_IF: return "MeasuredIf ";
    case RC_CONDITION_TRIGGER: return "Trigger ";
    case RC_CONDITION_MEASURED: return "Measured ";
    case RC_CONDITION_ADD_SOURCE: return "AddSource ";
    case RC_CONDITION_SUB_SOURCE: return "SubSource ";
    case RC_CONDITION_ADD_ADDRESS: return "AddAddress ";
    case RC_CONDITION_ADD_HITS: return "AddHits ";
    case RC_CONDITION_SUB_HITS: return "SubHits ";
    case RC_CONDITION_RESET_NEXT_IF: return "ResetNextIf ";
    case RC_CONDITION_AND_NEXT: return "AndNext ";
    case RC_CONDITION_OR_NEXT: return "OrNext ";
    default: return "Unknown";
  }
}

static const char* type_string(char type) {
  switch (type) {
    case RC_OPERAND_ADDRESS: return "Mem";
    case RC_OPERAND_DELTA: return "Delta";
    case RC_OPERAND_CONST: return "Value";
    case RC_OPERAND_FP: return "Float";
    case RC_OPERAND_LUA: return "Lua";
    case RC_OPERAND_PRIOR: return "Prior";
    case RC_OPERAND_BCD: return "BCD";
    case RC_OPERAND_INVERTED: return "Inverted";
    default: return "Unknown";
  }
}

static const char* size_string(char size) {
  switch (size) {
    case RC_MEMSIZE_8_BITS: return "8-bit";
    case RC_MEMSIZE_16_BITS: return "16-bit";
    case RC_MEMSIZE_24_BITS: return "24-bit";
    case RC_MEMSIZE_32_BITS: return "32-bit";
    case RC_MEMSIZE_LOW: return "Lower4";
    case RC_MEMSIZE_HIGH: return "Upper4";
    case RC_MEMSIZE_BIT_0: return "Bit0";
    case RC_MEMSIZE_BIT_1: return "Bit1";
    case RC_MEMSIZE_BIT_2: return "Bit2";
    case RC_MEMSIZE_BIT_3: return "Bit3";
    case RC_MEMSIZE_BIT_4: return "Bit4";
    case RC_MEMSIZE_BIT_5: return "Bit5";
    case RC_MEMSIZE_BIT_6: return "Bit6";
    case RC_MEMSIZE_BIT_7: return "Bit7";
    case RC_MEMSIZE_BITCOUNT: return "BitCount";
    case RC_MEMSIZE_16_BITS_BE: return "16-bit BE";
    case RC_MEMSIZE_24_BITS_BE: return "24-bit BE";
    case RC_MEMSIZE_32_BITS_BE: return "32-bit BE";
    case RC_MEMSIZE_VARIABLE: return "Variable";
    default: return "Unknown";
  }
}

static const char* operator_string(char oper) {
  switch (oper) {
    case RC_OPERATOR_NONE: return "";
    case RC_OPERATOR_AND: return "&";
    case RC_OPERATOR_MULT: return "*";
    case RC_OPERATOR_DIV: return "/";
    case RC_OPERATOR_EQ: return "=";
    case RC_OPERATOR_NE: return "!=";
    case RC_OPERATOR_GE: return ">=";
    case RC_OPERATOR_GT: return ">";
    case RC_OPERATOR_LE: return "<=";
    case RC_OPERATOR_LT: return "<";
    default: return "?";
  }
}

static void append_condition(char result[], size_t result_size, const rc_condition_t* cond) {
  const char* flag = flag_string(cond->type);
  const char* src_type = type_string(cond->operand1.type);
  const char* tgt_type = type_string(cond->operand2.type);
  const char* cmp = operator_string(cond->oper);
  char val1[32], val2[32];

  if (rc_operand_is_memref(&cond->operand1))
    snprintf(val1, sizeof(val1), "%s 0x%06x", size_string(cond->operand1.size), cond->operand1.value.memref->address);
  else
    snprintf(val1, sizeof(val1), "0x%06x", cond->operand1.value.num);

  if (rc_operand_is_memref(&cond->operand2))
    snprintf(val2, sizeof(val2), "%s 0x%06x", size_string(cond->operand2.size), cond->operand2.value.memref->address);
  else
    snprintf(val2, sizeof(val2), "0x%06x", cond->operand2.value.num);

  const size_t message_len = strlen(result);
  result += message_len;
  result_size -= message_len;

  if (cond->oper == RC_OPERATOR_NONE)
    snprintf(result, result_size, ": %s%s %s", flag, src_type, val1);
  else
    snprintf(result, result_size, ": %s%s %s %s %s %s", flag, src_type, val1, cmp, tgt_type, val2);
}

static void append_invalid_condition(char result[], size_t result_size, const rc_condset_t* condset) {
  if (strncmp(result, "Condition ", 10) == 0) {
    int index = atoi(&result[10]);
    const rc_condition_t* cond;
    for (cond = condset->conditions; cond; cond = cond->next) {
      if (--index == 0) {
        const size_t error_length = strlen(result);
        append_condition(result + error_length, result_size - error_length, cond);
        return;
      }
    }
  }
}

static void append_invalid_trigger_condition(char result[], size_t result_size, const rc_trigger_t* trigger) {
  if (strncmp(result, "Alt", 3) == 0) {
    int index = atoi(&result[3]);
    const rc_condset_t* condset;
    for (condset = trigger->alternative; condset; condset = condset->next) {
      if (--index == 0) {
        result += 4;
        result_size -= 4;
        while (isdigit(*result)) {
          ++result;
          --result_size;
        }
        ++result;
        --result_size;
        append_invalid_condition(result, result_size, condset);
        return;
      }
    }
  }
  else if (strncmp(result, "Core ", 5) == 0) {
    append_invalid_condition(result + 5, result_size - 5, trigger->requirement);
  }
  else {
    append_invalid_condition(result, result_size, trigger->requirement);
  }
}

static int validate_trigger(const char* trigger, char result[], size_t result_size, unsigned max_address) {
  char* buffer;
  rc_trigger_t* compiled;
  int success = 0;

  int ret = rc_trigger_size(trigger);
  if (ret < 0) {
    snprintf(result, result_size, "%s", rc_error_str(ret));
    return 0;
  }

  buffer = (char*)malloc(ret + 4);
  memset(buffer + ret, 0xCD, 4);
  compiled = rc_parse_trigger(buffer, trigger, NULL, 0);
  if (compiled == NULL) {
    snprintf(result, result_size, "parse failed");
  }
  else if (*(unsigned*)&buffer[ret] != 0xCDCDCDCD) {
    snprintf(result, result_size, "write past end of buffer");
  }
  else if (rc_validate_trigger(compiled, result, result_size, max_address)) {
    snprintf(result, result_size, "%d OK", ret);
    success = 1;
  }
  else {
    append_invalid_trigger_condition(result, result_size, compiled);
  }

  free(buffer);
  return success;
}

static int validate_leaderboard(const char* leaderboard, char result[], const size_t result_size, unsigned max_address)
{
  char* buffer;
  rc_lboard_t* compiled;
  int success = 0;

  int ret = rc_lboard_size(leaderboard);
  if (ret < 0) {
    /* generic problem parsing the leaderboard, attempt to report where */
    const char* start = leaderboard;
    char part[4] = { 0,0,0,0 };
    do {
      char* next = strstr(start, "::");
      part[0] = toupper((int)start[0]);
      part[1] = toupper((int)start[1]);
      part[2] = toupper((int)start[2]);
      start += 4;

      if (strcmp(part, "VAL") == 0) {
        int ret2 = rc_value_size(start);
        if (ret2 == ret) {
          snprintf(result, result_size, "%s: %s", part, rc_error_str(ret));
          return 0;
        }
      }
      else {
        int ret2 = rc_trigger_size(start);
        if (ret2 == ret) {
          snprintf(result, result_size, "%s: %s", part, rc_error_str(ret));
          return 0;
        }
      }

      if (!next)
        break;

      start = next + 2;
    } while (1);

    snprintf(result, result_size, "%s", rc_error_str(ret));
    return 0;
  }

  buffer = (char*)malloc(ret + 4);
  memset(buffer + ret, 0xCD, 4);
  compiled = rc_parse_lboard(buffer, leaderboard, NULL, 0);
  if (compiled == NULL) {
    snprintf(result, result_size, "parse failed");
  }
  else if (*(unsigned*)&buffer[ret] != 0xCDCDCDCD) {
    snprintf(result, result_size, "write past end of buffer");
  }
  else {
    snprintf(result, result_size, "STA: ");
    success = rc_validate_trigger(&compiled->start, result + 5, result_size - 5, max_address);
    if (!success) {
      append_invalid_trigger_condition(result + 5, result_size - 5, &compiled->start);
    }
    else {
      snprintf(result, result_size, "SUB: ");
      success = rc_validate_trigger(&compiled->submit, result + 5, result_size - 5, max_address);
      if (!success) {
        append_invalid_trigger_condition(result + 5, result_size - 5, &compiled->submit);
      }
      else {
        snprintf(result, result_size, "CAN: ");
        success = rc_validate_trigger(&compiled->cancel, result + 5, result_size - 5, max_address);

        if (!success) {
          append_invalid_trigger_condition(result + 5, result_size - 5, &compiled->cancel);
        }
        else {
          snprintf(result, result_size, "%d OK", ret);
        }
      }
    }
  }

  free(buffer);
  return success;
}

static int validate_macros(const rc_richpresence_t* richpresence, const char* script, char result[], const size_t result_size)
{
  const unsigned short RC_FORMAT_UNKNOWN_MACRO = 103; /* enum not exposed by header */

  rc_richpresence_display_t* display = richpresence->first_display;
  while (display != NULL) {
    rc_richpresence_display_part_t* part = display->display;
    while (part != NULL) {
      if (part->display_type == RC_FORMAT_UNKNOWN_MACRO) {
        /* include opening parenthesis to prevent partial match */
        size_t macro_len = strchr(part->text, '(') - part->text + 1;

        /* find the display portion of the script */
        const char* ptr = script;
        int line = 1;
        while (strncmp(ptr, "Display:", 8) != 0) {
          while (*ptr != '\n')
            ++ptr;

          ++line;
          ++ptr;
        }

        /* find the first matching reference to the unknown macro */
        do {
          while (*ptr != '@') {
            if (*ptr == '\n')
              ++line;

            if (*ptr == '\0') {
              /* unexpected, but prevent potential infinite loop */
              snprintf(result, result_size, "Unknown macro \"%.*s\"", (int)(macro_len - 1), part->text);
              return 0;
            }

            ++ptr;
          }
          ++ptr;

          if (strncmp(ptr, part->text, macro_len) == 0) {
            snprintf(result, result_size, "Line %d: Unknown macro \"%.*s\"", line, (int)(macro_len - 1), part->text);
            return 0;
          }
        } while (1);
      }

      part = part->next;
    }

    display = display->next;
  }

  return 1;
}

static int validate_richpresence(const char* script, char result[], const size_t result_size, unsigned max_address)
{
  char* buffer;
  rc_richpresence_t* compiled;
  int lines;
  int success = 0;

  int ret = rc_richpresence_size_lines(script, &lines);
  if (ret < 0) {
    snprintf(result, result_size, "Line %d: %s", lines, rc_error_str(ret));
    return 0;
  }

  buffer = (char*)malloc(ret + 4);
  memset(buffer + ret, 0xCD, 4);
  compiled = rc_parse_richpresence(buffer, script, NULL, 0);
  if (compiled == NULL) {
    snprintf(result, result_size, "parse failed");
  }
  else if (*(unsigned*)&buffer[ret] != 0xCDCDCDCD) {
    snprintf(result, result_size, "write past end of buffer");
  }
  else {
    const rc_richpresence_display_t* display;
    int index = 1;
    for (display = compiled->first_display; display; display = display->next) {
      const size_t prefix_length = snprintf(result, result_size, "Display%d: ", index++);
      success = rc_validate_trigger(&display->trigger, result + prefix_length, result_size - prefix_length, max_address);
      if (!success)
        break;
    }

    if (success)
      success = rc_validate_memrefs(compiled->memrefs, result, result_size, max_address);
    if (success)
      success = validate_macros(compiled, script, result, result_size);
    if (success)
      snprintf(result, result_size, "%d OK", ret);
  }

  free(buffer);
  return success;
}

static void validate_richpresence_file(const char* richpresence_file, char result[], const size_t result_size)
{
  char* file_contents;
  size_t file_size;
  FILE* file;

  file = fopen(richpresence_file, "rb");
  if (!file) {
    snprintf(result, result_size, "could not open file");
    return;
  }

  fseek(file, 0, SEEK_END);
  file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  file_contents = (char*)malloc(file_size + 1);
  fread(file_contents, 1, file_size, file);
  file_contents[file_size] = '\0';
  fclose(file);

  validate_richpresence(file_contents, result, result_size, 0xFFFFFFFF);

  free(file_contents);
}

static int validate_patchdata_file(const char* patchdata_file, const char* filename, int errors_only) {
  char* file_contents;
  size_t file_size;
  FILE* file;
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  const rc_memory_regions_t* memory_regions;
  int result;
  size_t i;
  char file_title[256];
  char buffer[256];
  int success = 1;
  unsigned max_address = 0xFFFFFFFF;

  file = fopen(patchdata_file, "rb");
  if (!file) {
    printf("File: %s: could not open file\n", filename);
    return 0;
  }

  fseek(file, 0, SEEK_END);
  file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  file_contents = (char*)malloc(file_size + 1);
  fread(file_contents, 1, file_size, file);
  file_contents[file_size] = '\0';
  fclose(file);

  result = rc_api_process_fetch_game_data_response(&fetch_game_data_response, file_contents);
  if (result != RC_OK) {
    printf("File: %s: %s\n", filename, rc_error_str(result));
    return 0;
  }

  free(file_contents);

  snprintf(file_title, sizeof(file_title), "File: %s: %s\n", filename, fetch_game_data_response.title);

  memory_regions = rc_console_memory_regions(fetch_game_data_response.console_id);
  if (memory_regions && memory_regions->num_regions > 0)
    max_address = memory_regions->region[memory_regions->num_regions - 1].end_address;

  if (fetch_game_data_response.rich_presence_script && *fetch_game_data_response.rich_presence_script) {
    result = validate_richpresence(fetch_game_data_response.rich_presence_script, 
                                   buffer, sizeof(buffer), max_address);
    success &= result;

    if (!result || !errors_only) {
      printf("%s", file_title);
      file_title[0] = '\0';

      printf(" rich presence %d: %s\n", fetch_game_data_response.id, buffer);
    }
  }

  for (i = 0; i < fetch_game_data_response.num_achievements; ++i) {
    const char* trigger = fetch_game_data_response.achievements[i].definition;
    result = validate_trigger(trigger, buffer, sizeof(buffer), max_address);
    success &= result;

    if (!result || !errors_only) {
      if (file_title[0]) {
        printf("%s", file_title);
        file_title[0] = '\0';
      }

      printf(" achievement %d%s: %s\n", fetch_game_data_response.achievements[i].id,
          (fetch_game_data_response.achievements[i].category == 3) ? "" : " (Unofficial)", buffer);
    }
  }

  for (i = 0; i < fetch_game_data_response.num_leaderboards; ++i) {
    result = validate_leaderboard(fetch_game_data_response.leaderboards[i].definition, 
                                  buffer, sizeof(buffer), max_address);
    success &= result;

    if (!result || !errors_only) {
      if (file_title[0]) {
        printf("%s", file_title);
        file_title[0] = '\0';
      }

      printf(" leaderboard %d: %s\n", fetch_game_data_response.leaderboards[i].id, buffer);
    }
  }

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);

  return success;
}

#ifdef _CRT_SECURE_NO_WARNINGS
static void validate_patchdata_directory(const char* patchdata_directory, int errors_only) {
  WIN32_FIND_DATA fdFile;
  HANDLE hFind = NULL;
  int need_newline = 0;

  char filename[MAX_PATH];
  sprintf(filename, "%s\\*.json", patchdata_directory);

  if ((hFind = FindFirstFile(filename, &fdFile)) == INVALID_HANDLE_VALUE) {
    printf("failed to open directory");
    return;
  }

  do
  {
    if (!(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      if (need_newline) {
        printf("\n");
        need_newline = 0;
      }

      sprintf(filename, "%s\\%s", patchdata_directory, fdFile.cFileName);
      if (!validate_patchdata_file(filename, fdFile.cFileName, errors_only) || !errors_only)
        need_newline = 1;
    }
  } while(FindNextFile(hFind, &fdFile));

  FindClose(hFind);
}
#else
static void validate_patchdata_directory(const char* patchdata_directory, int errors_only) {
  struct dirent* entry;
  char* filename;
  size_t filename_len;
  char path[2048];
  int need_newline = 0;

  DIR* dir = opendir(patchdata_directory);
  if (!dir) {
    printf("failed to open directory");
    return;
  }

  while ((entry = readdir(dir)) != NULL) {
    filename = entry->d_name;
    filename_len = strlen(filename);
    if (filename_len > 5 && stricmp(&filename[filename_len - 5], ".json") == 0) {
      if (need_newline) {
        printf("\n");
        need_newline = 0;
      }

      sprintf(path, "%s/%s", patchdata_directory, filename);
      if (!validate_patchdata_file(path, filename, errors_only) || !errors_only)
        need_newline = 1;
    }
  }

  closedir(dir);
}
#endif

static int usage() {
  printf("validator [type] [data]\n"
         "\n"
         "where [type] is one of the following:\n"
         "  a   achievement, [data] = trigger definition\n"
         "  l   leaderboard, [data] = leaderboard definition\n"
         "  r   rich presence, [data] = path to rich presence script\n"
         "  f   patchdata file, [data] = path to patchdata json file\n"
         "  d   patchdata directory, [data] = path to directory containing one or more patchdata json files\n"
         "  e   same as 'd', but only reports errors\n"
  );

  return 0;
}

int main(int argc, char* argv[]) {
  char buffer[256];

  if (argc < 3)
    return usage();

  switch (argv[1][0])
  {
    case 'a':
      validate_trigger(argv[2], buffer, sizeof(buffer), 0xFFFFFFFF);
      printf("Achievement: %s\n", buffer);
      break;

    case 'l':
      validate_leaderboard(argv[2], buffer, sizeof(buffer), 0xFFFFFFFF);
      printf("Leaderboard: %s\n", buffer);
      break;

    case 'r':
      validate_richpresence_file(argv[2], buffer, sizeof(buffer));
      printf("Rich Presence: %s\n", buffer);
      break;

    case 'f':
      validate_patchdata_file(argv[2], argv[2], 0);
      break;

    case 'd':
      printf("Directory: %s:\n", argv[2]);
      validate_patchdata_directory(argv[2], 0);
      break;

    case 'e':
      printf("Directory: %s:\n", argv[2]);
      validate_patchdata_directory(argv[2], 1);
      break;

    default:
      return usage();
  }

  return 0;
}
