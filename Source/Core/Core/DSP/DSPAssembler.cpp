// Copyright 2009 Dolphin Emulator Project
// Copyright 2005 Duddie, wntrmute, Hermes
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/DSP/DSPAssembler.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include "Core/DSP/DSPDisassembler.h"
#include "Core/DSP/DSPTables.h"

namespace DSP
{
static const char* err_string[] = {"",
                                   "Unknown Error",
                                   "Unknown opcode",
                                   "Not enough parameters",
                                   "Too many parameters",
                                   "Wrong parameter",
                                   "Expected parameter of type 'string'",
                                   "Expected parameter of type 'value'",
                                   "Expected parameter of type 'register'",
                                   "Expected parameter of type 'memory pointer'",
                                   "Expected parameter of type 'immediate'",
                                   "Incorrect binary value",
                                   "Incorrect hexadecimal value",
                                   "Incorrect decimal value",
                                   "Label already exists",
                                   "Label not defined",
                                   "No matching brackets",
                                   "This opcode cannot be extended",
                                   "Given extending params for non extensible opcode",
                                   "Wrong parameter: must be accumulator register",
                                   "Wrong parameter: must be mid accumulator register",
                                   "Invalid register",
                                   "Number out of range",
                                   "Program counter out of range"};

DSPAssembler::DSPAssembler(const AssemblerSettings& settings) : m_settings(settings)
{
}

DSPAssembler::~DSPAssembler() = default;

bool DSPAssembler::Assemble(const std::string& text, std::vector<u16>& code,
                            std::vector<int>* line_numbers)
{
  if (line_numbers)
    line_numbers->clear();
  InitPass(1);
  if (!AssemblePass(text, 1))
    return false;

  // We now have the size of the output buffer
  if (m_total_size <= 0)
    return false;

  m_output_buffer.resize(m_total_size);

  InitPass(2);
  if (!AssemblePass(text, 2))
    return false;

  code = std::move(m_output_buffer);
  m_output_buffer.clear();
  m_output_buffer.shrink_to_fit();

  m_last_error_str = "(no errors)";
  m_last_error = AssemblerError::OK;

  return true;
}

void DSPAssembler::ShowError(AssemblerError err_code, const char* extra_info)
{
  if (!m_settings.force)
    m_failed = true;

  std::string error = StringFromFormat("%u : %s ", m_code_line, m_cur_line.c_str());
  if (!extra_info)
    extra_info = "-";

  const char* const error_string = err_string[static_cast<size_t>(err_code)];

  if (m_current_param == 0)
  {
    error += StringFromFormat("ERROR: %s Line: %u : %s\n", error_string, m_code_line, extra_info);
  }
  else
  {
    error += StringFromFormat("ERROR: %s Line: %u Param: %d : %s\n", error_string, m_code_line,
                              m_current_param, extra_info);
  }

  m_last_error_str = std::move(error);
  m_last_error = err_code;
}

static char* skip_spaces(char* ptr)
{
  while (*ptr == ' ')
    ptr++;
  return ptr;
}

// Parse a standalone value - it can be a number in one of several formats or a label.
s32 DSPAssembler::ParseValue(const char* str)
{
  bool negative = false;
  s32 val = 0;
  const char* ptr = str;

  if (ptr[0] == '#')
  {
    ptr++;
    negative = true;  // Wow! Double # (needed one to get in here) negates???
  }
  if (ptr[0] == '-')
  {
    ptr++;
    negative = true;
  }
  if (ptr[0] == '0')
  {
    if (ptr[1] >= '0' && ptr[1] <= '9')
    {
      for (int i = 0; ptr[i] != 0; i++)
      {
        val *= 10;
        if (ptr[i] >= '0' && ptr[i] <= '9')
          val += ptr[i] - '0';
        else
          ShowError(AssemblerError::IncorrectDecimal, str);
      }
    }
    else
    {
      switch (ptr[1])
      {
      case 'X':  // hex
        for (int i = 2; ptr[i] != 0; i++)
        {
          val <<= 4;
          if (ptr[i] >= 'a' && ptr[i] <= 'f')
            val += (ptr[i] - 'a' + 10);
          else if (ptr[i] >= 'A' && ptr[i] <= 'F')
            val += (ptr[i] - 'A' + 10);
          else if (ptr[i] >= '0' && ptr[i] <= '9')
            val += (ptr[i] - '0');
          else
            ShowError(AssemblerError::IncorrectHex, str);
        }
        break;
      case '\'':  // binary
        for (int i = 2; ptr[i] != 0; i++)
        {
          val *= 2;
          if (ptr[i] >= '0' && ptr[i] <= '1')
            val += ptr[i] - '0';
          else
            ShowError(AssemblerError::IncorrectBinary, str);
        }
        break;
      default:
        // value is 0 or error
        val = 0;
        break;
      }
    }
  }
  else
  {
    // Symbol starts with a digit - it's a dec number.
    if (ptr[0] >= '0' && ptr[0] <= '9')
    {
      for (int i = 0; ptr[i] != 0; i++)
      {
        val *= 10;
        if (ptr[i] >= '0' && ptr[i] <= '9')
          val += ptr[i] - '0';
        else
          ShowError(AssemblerError::IncorrectDecimal, str);
      }
    }
    else  // Everything else is a label.
    {
      // Lookup label
      if (const std::optional<u16> value = m_labels.GetLabelValue(ptr))
        return *value;

      if (m_cur_pass == 2)
        ShowError(AssemblerError::UnknownLabel, str);
    }
  }
  if (negative)
    return -val;
  return val;
}

// This function splits the given src string into three parts:
// - Text before the first opening ('(') parenthesis
// - Text within the first and last opening ('(') and closing (')') parentheses.
// - If text follows after these parentheses, then this is what is returned from the function.
//
// Note that the first opening parenthesis and the last closing parenthesis are discarded from the
// string.
// For example: Say "Test (string) 1234" is the string passed in as src.
//
// - src will become "Test "
// - dst will become "string"
// - Returned string from the function will be " 1234"
//
char* DSPAssembler::FindBrackets(char* src, char* dst)
{
  s32 len = (s32)strlen(src);
  s32 first = -1;
  s32 count = 0;
  s32 i, j;
  j = 0;
  for (i = 0; i < len; i++)
  {
    if (src[i] == '(')
    {
      if (first < 0)
      {
        count = 1;
        src[i] = 0x0;
        first = i;
      }
      else
      {
        count++;
        dst[j++] = src[i];
      }
    }
    else if (src[i] == ')')
    {
      if (--count == 0)
      {
        dst[j] = 0;
        return &src[i + 1];
      }
      else
      {
        dst[j++] = src[i];
      }
    }
    else
    {
      if (first >= 0)
        dst[j++] = src[i];
    }
  }
  if (count)
    ShowError(AssemblerError::NoMatchingBrackets);
  return nullptr;
}

// Bizarre in-place expression evaluator.
u32 DSPAssembler::ParseExpression(const char* ptr)
{
  char* pbuf;
  s32 val = 0;

  char* d_buffer = (char*)malloc(1024);
  char* s_buffer = (char*)malloc(1024);
  strcpy(s_buffer, ptr);

  while ((pbuf = FindBrackets(s_buffer, d_buffer)) != nullptr)
  {
    val = ParseExpression(d_buffer);
    sprintf(d_buffer, "%s%d%s", s_buffer, val, pbuf);
    strcpy(s_buffer, d_buffer);
  }

  int j = 0;
  for (int i = 0; i < ((s32)strlen(s_buffer) + 1); i++)
  {
    char c = s_buffer[i];
    if (c != ' ')
      d_buffer[j++] = c;
  }

  for (int i = 0; i < ((s32)strlen(d_buffer) + 1); i++)
  {
    char c = d_buffer[i];
    if (c == '-')
    {
      if (i == 0)
        c = '#';
      else
      {
        switch (d_buffer[i - 1])
        {
        case '/':
        case '%':
        case '*':
          c = '#';
        }
      }
    }
    d_buffer[i] = c;
  }

  while ((pbuf = strstr(d_buffer, "+")) != nullptr)
  {
    *pbuf = 0x0;
    val = ParseExpression(d_buffer) + ParseExpression(pbuf + 1);
    sprintf(d_buffer, "%d", val);
  }

  while ((pbuf = strstr(d_buffer, "-")) != nullptr)
  {
    *pbuf = 0x0;
    val = ParseExpression(d_buffer) - ParseExpression(pbuf + 1);
    if (val < 0)
    {
      val = 0x10000 +
            (val &
             0xffff);  // ATTENTION: avoid a terrible bug!!! number cannot write with '-' in sprintf
      fprintf(stderr, "WARNING: Number Underflow at Line: %d \n", m_code_line);
    }
    sprintf(d_buffer, "%d", val);
  }

  while ((pbuf = strstr(d_buffer, "*")) != nullptr)
  {
    *pbuf = 0x0;
    val = ParseExpression(d_buffer) * ParseExpression(pbuf + 1);
    sprintf(d_buffer, "%d", val);
  }

  while ((pbuf = strstr(d_buffer, "/")) != nullptr)
  {
    *pbuf = 0x0;
    val = ParseExpression(d_buffer) / ParseExpression(pbuf + 1);
    sprintf(d_buffer, "%d", val);
  }

  while ((pbuf = strstr(d_buffer, "|")) != nullptr)
  {
    *pbuf = 0x0;
    val = ParseExpression(d_buffer) | ParseExpression(pbuf + 1);
    sprintf(d_buffer, "%d", val);
  }

  while ((pbuf = strstr(d_buffer, "&")) != nullptr)
  {
    *pbuf = 0x0;
    val = ParseExpression(d_buffer) & ParseExpression(pbuf + 1);
    sprintf(d_buffer, "%d", val);
  }

  val = ParseValue(d_buffer);
  free(d_buffer);
  free(s_buffer);
  return val;
}

// Destroys parstr
u32 DSPAssembler::GetParams(char* parstr, param_t* par)
{
  u32 count = 0;
  char* tmpstr = skip_spaces(parstr);
  tmpstr = strtok(tmpstr, ",\x00");
  for (int i = 0; i < 10; i++)
  {
    if (tmpstr == nullptr)
      break;
    tmpstr = skip_spaces(tmpstr);
    if (strlen(tmpstr) == 0)
      break;
    if (tmpstr)
      count++;
    else
      break;

    par[i].type = P_NONE;
    switch (tmpstr[0])
    {
    case '"':
      par[i].str = strtok(tmpstr, "\"");
      par[i].type = P_STR;
      break;
    case '#':
      par[i].val = ParseExpression(tmpstr + 1);
      par[i].type = P_IMM;
      break;
    case '@':
      if (tmpstr[1] == '$')
      {
        par[i].val = ParseExpression(tmpstr + 2);
        par[i].type = P_PRG;
      }
      else
      {
        par[i].val = ParseExpression(tmpstr + 1);
        par[i].type = P_MEM;
      }
      break;
    case '$':
      par[i].val = ParseExpression(tmpstr + 1);
      par[i].type = P_REG;
      break;

    default:
      par[i].val = ParseExpression(tmpstr);
      par[i].type = P_VAL;
      break;
    }
    tmpstr = strtok(nullptr, ",\x00");
  }
  return count;
}

const DSPOPCTemplate* DSPAssembler::FindOpcode(std::string name, size_t par_count, OpcodeType type)
{
  if (name[0] == 'C' && name[1] == 'W')
    return &cw;

  const auto alias_iter = m_aliases.find(name);
  if (alias_iter != m_aliases.end())
    name = alias_iter->second;

  const DSPOPCTemplate* const info =
      type == OpcodeType::Primary ? FindOpInfoByName(name) : FindExtOpInfoByName(name);
  if (!info)
  {
    ShowError(AssemblerError::UnknownOpcode);
    return nullptr;
  }

  if (par_count < info->param_count)
  {
    ShowError(AssemblerError::NotEnoughParameters);
  }
  else if (par_count > info->param_count)
  {
    ShowError(AssemblerError::TooManyParameters);
  }

  return info;
}

// weird...
static u16 get_mask_shifted_down(u16 mask)
{
  while (!(mask & 1))
    mask >>= 1;
  return mask;
}

bool DSPAssembler::VerifyParams(const DSPOPCTemplate* opc, param_t* par, size_t count,
                                OpcodeType type)
{
  for (size_t i = 0; i < count; i++)
  {
    const size_t current_param = i + 1;  // just for display.
    if (opc->params[i].type != par[i].type || (par[i].type & P_REG))
    {
      if (par[i].type == P_VAL &&
          (opc->params[i].type == P_ADDR_I || opc->params[i].type == P_ADDR_D))
      {
        // Data and instruction addresses are valid as VAL values.
        continue;
      }

      if ((opc->params[i].type & P_REG) && (par[i].type & P_REG))
      {
        // Just a temp. Should be replaced with more purposeful vars.
        int value;

        // modified by Hermes: test the register range
        switch ((unsigned)opc->params[i].type)
        {
        case P_REG18:
        case P_REG19:
        case P_REG1A:
        case P_REG1C:
          value = (opc->params[i].type >> 8) & 31;
          if ((int)par[i].val < value ||
              (int)par[i].val > value + get_mask_shifted_down(opc->params[i].mask))
          {
            if (type == OpcodeType::Extension)
              fprintf(stderr, "(ext) ");

            fprintf(stderr, "%s   (param %zu)", m_cur_line.c_str(), current_param);
            ShowError(AssemblerError::InvalidRegister);
          }
          break;
        case P_PRG:
          if ((int)par[i].val < 0 || (int)par[i].val > 0x3)
          {
            if (type == OpcodeType::Extension)
              fprintf(stderr, "(ext) ");

            fprintf(stderr, "%s   (param %zu)", m_cur_line.c_str(), current_param);
            ShowError(AssemblerError::InvalidRegister);
          }
          break;
        case P_ACC:
          if ((int)par[i].val < 0x20 || (int)par[i].val > 0x21)
          {
            if (type == OpcodeType::Extension)
              fprintf(stderr, "(ext) ");

            if (par[i].val >= 0x1e && par[i].val <= 0x1f)
            {
              fprintf(stderr, "%i : %s ", m_code_line, m_cur_line.c_str());
              fprintf(stderr,
                      "WARNING: $ACM%d register used instead of $ACC%d register Line: %d "
                      "Param: %zu Ext: %d\n",
                      (par[i].val & 1), (par[i].val & 1), m_code_line, current_param,
                      static_cast<int>(type));
            }
            else if (par[i].val >= 0x1c && par[i].val <= 0x1d)
            {
              fprintf(
                  stderr,
                  "WARNING: $ACL%d register used instead of $ACC%d register Line: %d Param: %zu\n",
                  (par[i].val & 1), (par[i].val & 1), m_code_line, current_param);
            }
            else
            {
              ShowError(AssemblerError::WrongParameterExpectedAccumulator);
            }
          }
          break;
        case P_ACCM:
          if ((int)par[i].val < 0x1e || (int)par[i].val > 0x1f)
          {
            if (type == OpcodeType::Extension)
              fprintf(stderr, "(ext) ");

            if (par[i].val >= 0x1c && par[i].val <= 0x1d)
            {
              fprintf(
                  stderr,
                  "WARNING: $ACL%d register used instead of $ACM%d register Line: %d Param: %zu\n",
                  (par[i].val & 1), (par[i].val & 1), m_code_line, current_param);
            }
            else if (par[i].val >= 0x20 && par[i].val <= 0x21)
            {
              fprintf(
                  stderr,
                  "WARNING: $ACC%d register used instead of $ACM%d register Line: %d Param: %zu\n",
                  (par[i].val & 1), (par[i].val & 1), m_code_line, current_param);
            }
            else
            {
              ShowError(AssemblerError::WrongParameterExpectedAccumulator);
            }
          }
          break;

        case P_ACCL:
          if ((int)par[i].val < 0x1c || (int)par[i].val > 0x1d)
          {
            if (type == OpcodeType::Extension)
              fprintf(stderr, "(ext) ");

            if (par[i].val >= 0x1e && par[i].val <= 0x1f)
            {
              fprintf(stderr, "%s ", m_cur_line.c_str());
              fprintf(
                  stderr,
                  "WARNING: $ACM%d register used instead of $ACL%d register Line: %d Param: %zu\n",
                  (par[i].val & 1), (par[i].val & 1), m_code_line, current_param);
            }
            else if (par[i].val >= 0x20 && par[i].val <= 0x21)
            {
              fprintf(stderr, "%s ", m_cur_line.c_str());
              fprintf(
                  stderr,
                  "WARNING: $ACC%d register used instead of $ACL%d register Line: %d Param: %zu\n",
                  (par[i].val & 1), (par[i].val & 1), m_code_line, current_param);
            }
            else
            {
              ShowError(AssemblerError::WrongParameterExpectedAccumulator);
            }
          }
          break;
        }
        continue;
      }

      switch (par[i].type & (P_REG | 7))
      {
      case P_REG:
        if (type == OpcodeType::Extension)
          fprintf(stderr, "(ext) ");
        ShowError(AssemblerError::ExpectedParamReg);
        break;
      case P_MEM:
        if (type == OpcodeType::Extension)
          fprintf(stderr, "(ext) ");
        ShowError(AssemblerError::ExpectedParamMem);
        break;
      case P_VAL:
        if (type == OpcodeType::Extension)
          fprintf(stderr, "(ext) ");
        ShowError(AssemblerError::ExpectedParamVal);
        break;
      case P_IMM:
        if (type == OpcodeType::Extension)
          fprintf(stderr, "(ext) ");
        ShowError(AssemblerError::ExpectedParamImm);
        break;
      }
      ShowError(AssemblerError::WrongParameter);
      break;
    }
    else if ((opc->params[i].type & 3) != 0 && (par[i].type & 3) != 0)
    {
      // modified by Hermes: test NUMBER range
      int value = get_mask_shifted_down(opc->params[i].mask);
      unsigned int valueu = 0xffff & ~(value >> 1);
      if ((int)par[i].val < 0)
      {
        if (value == 7)  // value 7 por sbclr/sbset
        {
          fprintf(stderr, "Value must be from 0x0 to 0x%x\n", value);
          ShowError(AssemblerError::NumberOutOfRange);
        }
        else if (opc->params[i].type == P_MEM)
        {
          if (value < 256)
            fprintf(stderr, "Address value must be from 0x%x to 0x%x\n", valueu, (value >> 1));
          else
            fprintf(stderr, "Address value must be from 0x0 to 0x%x\n", value);

          ShowError(AssemblerError::NumberOutOfRange);
        }
        else if ((int)par[i].val < -((value >> 1) + 1))
        {
          if (value < 128)
            fprintf(stderr, "Value must be from -0x%x to 0x%x, is %i\n", (value >> 1) + 1,
                    value >> 1, par[i].val);
          else
            fprintf(stderr, "Value must be from -0x%x to 0x%x or 0x0 to 0x%x, is %i\n",
                    (value >> 1) + 1, value >> 1, value, par[i].val);

          ShowError(AssemblerError::NumberOutOfRange);
        }
      }
      else
      {
        if (value == 7)  // value 7 por sbclr/sbset
        {
          if (par[i].val > (unsigned)value)
          {
            fprintf(stderr, "Value must be from 0x%x to 0x%x, is %i\n", valueu, value, par[i].val);
            ShowError(AssemblerError::NumberOutOfRange);
          }
        }
        else if (opc->params[i].type == P_MEM)
        {
          if (value < 256)
            value >>= 1;  // addressing 8 bit with sign
          if (par[i].val > (unsigned)value &&
              (par[i].val < valueu || par[i].val > (unsigned)0xffff))
          {
            if (value < 256)
              fprintf(stderr, "Address value must be from 0x%x to 0x%x, is %04x\n", valueu, value,
                      par[i].val);
            else
              fprintf(stderr, "Address value must be minor of 0x%x\n", value + 1);
            ShowError(AssemblerError::NumberOutOfRange);
          }
        }
        else
        {
          if (value < 128)
            value >>= 1;  // special case ASL/ASR/LSL/LSR
          if (par[i].val > (unsigned)value)
          {
            if (value < 64)
              fprintf(stderr, "Value must be from -0x%x to 0x%x, is %i\n", (value + 1), value,
                      par[i].val);
            else
              fprintf(stderr, "Value must be minor of 0x%x, is %i\n", value + 1, par[i].val);
            ShowError(AssemblerError::NumberOutOfRange);
          }
        }
      }
      continue;
    }
  }
  m_current_param = 0;
  return true;
}

// Merge opcode with params.
void DSPAssembler::BuildCode(const DSPOPCTemplate* opc, param_t* par, u32 par_count, u16* outbuf)
{
  outbuf[m_cur_addr] |= opc->opcode;
  for (u32 i = 0; i < par_count; i++)
  {
    // Ignore the "reverse" parameters since they are implicit.
    if (opc->params[i].type != P_ACC_D && opc->params[i].type != P_ACCM_D)
    {
      u16 t16 = outbuf[m_cur_addr + opc->params[i].loc];
      u16 v16 = par[i].val;
      if (opc->params[i].lshift > 0)
        v16 <<= opc->params[i].lshift;
      else
        v16 >>= -opc->params[i].lshift;
      v16 &= opc->params[i].mask;
      outbuf[m_cur_addr + opc->params[i].loc] = t16 | v16;
    }
  }
}

void DSPAssembler::InitPass(int pass)
{
  m_failed = false;
  if (pass == 1)
  {
    // Reset label table. Pre-populate with hw addresses and registers.
    m_labels.Clear();
    m_labels.RegisterDefaults();
    m_aliases.clear();
    m_aliases["S15"] = "SET15";
    m_aliases["S16"] = "SET16";
    m_aliases["S40"] = "SET40";
  }
  m_cur_addr = 0;
  m_total_size = 0;
  m_cur_segment = SEGMENT_CODE;
  m_segment_addr[SEGMENT_CODE] = 0;
  m_segment_addr[SEGMENT_DATA] = 0;
  m_segment_addr[SEGMENT_OVERLAY] = 0;
}

bool DSPAssembler::AssemblePass(const std::string& text, int pass)
{
  int disable_text = 0;  // modified by Hermes

  std::istringstream fsrc(text);

  m_code_line = 0;
  m_cur_pass = pass;

#define LINEBUF_SIZE 1024
  char line[LINEBUF_SIZE] = {0};
  while (!m_failed && !fsrc.fail() && !fsrc.eof())
  {
    int opcode_size = 0;
    fsrc.getline(line, LINEBUF_SIZE);
    if (fsrc.fail())
      break;

    m_cur_line = line;
    m_code_line++;

    param_t params[10] = {{0, P_NONE, nullptr}};
    param_t params_ext[10] = {{0, P_NONE, nullptr}};

    bool upper = true;
    for (int i = 0; i < LINEBUF_SIZE; i++)
    {
      char c = line[i];
      // This stuff handles /**/ and // comments.
      // modified by Hermes : added // and /* */ for long commentaries
      if (c == '/')
      {
        if (i < 1023)
        {
          if (line[i + 1] == '/')
            c = 0x00;
          else if (line[i + 1] == '*')
          {
            // toggle comment mode.
            disable_text = !disable_text;
          }
        }
      }
      else if (c == '*')
      {
        if (i < 1023 && line[i + 1] == '/' && disable_text)
        {
          disable_text = 0;
          c = 32;
          line[i + 1] = 32;
        }
      }

      // turn text into spaces if disable_text is on (in a comment).
      if (disable_text && ((unsigned char)c) > 32)
        c = 32;

      if (c == 0x0a || c == 0x0d || c == ';')
        c = 0x00;
      if (c == 0x09)  // tabs to spaces
        c = ' ';
      if (c == '"')
        upper = !upper;
      if (upper && c >= 'a' && c <= 'z')  // convert to uppercase
        c = c - 'a' + 'A';
      line[i] = c;
      if (c == 0)
        break;  // modified by Hermes
    }
    char* ptr = line;

    std::string label;

    size_t col_pos = std::string(line).find(":");
    if (col_pos != std::string::npos)
    {
      bool valid = true;

      for (int j = 0; j < (int)col_pos; j++)
      {
        if (j == 0)
          if (!((ptr[j] >= 'A' && ptr[j] <= 'Z') || (ptr[j] == '_')))
            valid = false;
        if (!((ptr[j] >= '0' && ptr[j] <= '9') || (ptr[j] >= 'A' && ptr[j] <= 'Z') ||
              (ptr[j] == '_')))
          valid = false;
      }
      if (valid)
      {
        label = std::string(line).substr(0, col_pos);
        ptr += col_pos + 1;
      }
    }

    char* opcode = strtok(ptr, " ");
    char* opcode_ext = nullptr;

    u32 params_count = 0;
    u32 params_count_ext = 0;
    if (opcode)
    {
      if ((opcode_ext = strstr(opcode, "'")) != nullptr)
      {
        opcode_ext[0] = '\0';
        opcode_ext++;
        if (strlen(opcode_ext) == 0)
          opcode_ext = nullptr;
      }
      // now we have opcode and label

      params_count = 0;
      params_count_ext = 0;

      char* paramstr = strtok(nullptr, "\0");
      char* paramstr_ext = nullptr;
      // there is valid opcode so probably we have parameters

      if (paramstr)
      {
        if ((paramstr_ext = strstr(paramstr, ":")) != nullptr)
        {
          paramstr_ext[0] = '\0';
          paramstr_ext++;
        }
      }

      if (paramstr)
        params_count = GetParams(paramstr, params);
      if (paramstr_ext)
        params_count_ext = GetParams(paramstr_ext, params_ext);
    }

    if (!label.empty())
    {
      // there is a valid label so lets store it in labels table
      u32 lval = m_cur_addr;
      if (opcode)
      {
        if (strcmp(opcode, "EQU") == 0)
        {
          lval = params[0].val;
          opcode = nullptr;
        }
      }
      if (pass == 1)
        m_labels.RegisterLabel(label, lval);
    }

    if (opcode == nullptr)
      continue;

    // check if opcode is reserved compiler word
    if (strcmp("INCLUDE", opcode) == 0)
    {
      if (params[0].type == P_STR)
      {
        std::string include_file_path;
        const u32 this_code_line = m_code_line;

        if (m_include_dir.empty())
        {
          include_file_path = params[0].str;
        }
        else
        {
          include_file_path = m_include_dir + '/' + params[0].str;
        }

        std::string included_text;
        File::ReadFileToString(include_file_path, included_text);
        AssemblePass(included_text, pass);

        m_code_line = this_code_line;
      }
      else
      {
        ShowError(AssemblerError::ExpectedParamStr);
      }
      continue;
    }

    if (strcmp("INCDIR", opcode) == 0)
    {
      if (params[0].type == P_STR)
        m_include_dir = params[0].str;
      else
        ShowError(AssemblerError::ExpectedParamStr);
      continue;
    }

    if (strcmp("ORG", opcode) == 0)
    {
      if (params[0].type == P_VAL)
      {
        m_total_size = std::max(m_cur_addr, params[0].val);
        m_cur_addr = params[0].val;
      }
      else
      {
        ShowError(AssemblerError::ExpectedParamVal);
      }
      continue;
    }

    if (strcmp("WARNPC", opcode) == 0)
    {
      if (params[0].type == P_VAL)
      {
        if (m_cur_addr > params[0].val)
        {
          std::string msg = StringFromFormat("WARNPC at 0x%04x, expected 0x%04x or less",
                                             m_cur_addr, params[0].val);
          ShowError(AssemblerError::PCOutOfRange, msg.c_str());
        }
      }
      else
      {
        ShowError(AssemblerError::ExpectedParamVal);
      }
      continue;
    }

    if (strcmp("SEGMENT", opcode) == 0)
    {
      if (params[0].type == P_STR)
      {
        m_segment_addr[m_cur_segment] = m_cur_addr;
        if (strcmp("DATA", params[0].str) == 0)
          m_cur_segment = SEGMENT_DATA;
        if (strcmp("CODE", params[0].str) == 0)
          m_cur_segment = SEGMENT_CODE;
        m_cur_addr = m_segment_addr[m_cur_segment];
      }
      else
        ShowError(AssemblerError::ExpectedParamStr);
      continue;
    }

    const DSPOPCTemplate* opc = FindOpcode(opcode, params_count, OpcodeType::Primary);
    if (!opc)
      opc = &cw;

    opcode_size = opc->size;

    VerifyParams(opc, params, params_count, OpcodeType::Primary);

    const DSPOPCTemplate* opc_ext = nullptr;
    // Check for opcode extensions.
    if (opc->extended)
    {
      if (opcode_ext)
      {
        opc_ext = FindOpcode(opcode_ext, params_count_ext, OpcodeType::Extension);
        VerifyParams(opc_ext, params_ext, params_count_ext, OpcodeType::Extension);
      }
      else if (params_count_ext)
      {
        ShowError(AssemblerError::ExtensionParamsOnNonExtendableOpcode);
      }
    }
    else
    {
      if (opcode_ext)
        ShowError(AssemblerError::CantExtendOpcode);
      if (params_count_ext)
        ShowError(AssemblerError::ExtensionParamsOnNonExtendableOpcode);
    }

    if (pass == 2)
    {
      // generate binary
      m_output_buffer[m_cur_addr] = 0x0000;
      BuildCode(opc, params, params_count, m_output_buffer.data());
      if (opc_ext)
        BuildCode(opc_ext, params_ext, params_count_ext, m_output_buffer.data());
    }

    m_cur_addr += opcode_size;
    m_total_size += opcode_size;
  };

  return !m_failed;
}
}  // namespace DSP
