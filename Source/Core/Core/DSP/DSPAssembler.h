// Copyright 2009 Dolphin Emulator Project
// Copyright 2005 Duddie
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <fmt/format.h>

#include "Common/CommonTypes.h"

#include "Core/DSP/DSPDisassembler.h"
#include "Core/DSP/DSPTables.h"
#include "Core/DSP/LabelMap.h"

namespace DSP
{
enum class AssemblerError
{
  OK,
  Unknown,
  UnknownOpcode,
  NotEnoughParameters,
  TooManyParameters,
  WrongParameter,
  ExpectedParamStr,
  ExpectedParamVal,
  ExpectedParamReg,
  ExpectedParamMem,
  ExpectedParamImm,
  IncorrectBinary,
  IncorrectHex,
  IncorrectDecimal,
  LabelAlreadyExists,
  UnknownLabel,
  NoMatchingBrackets,
  CantExtendOpcode,
  ExtensionParamsOnNonExtendableOpcode,
  WrongParameterExpectedAccumulator,
  WrongParameterExpectedMidAccumulator,
  InvalidRegister,
  NumberOutOfRange,
  PCOutOfRange,
};

// Unless you want labels to carry over between files, you probably
// want to create a new DSPAssembler for every file you assemble.
class DSPAssembler
{
public:
  explicit DSPAssembler(const AssemblerSettings& settings);
  ~DSPAssembler();

  // line_numbers is optional (and not yet implemented). It'll receieve a list of ints,
  // one for each word of code, indicating the source assembler code line number it came from.

  // If returns false, call GetErrorString to get some text to present to the user.
  bool Assemble(const std::string& text, std::vector<u16>& code,
                std::vector<int>* line_numbers = nullptr);

  std::string GetErrorString() const { return m_last_error_str; }
  AssemblerError GetError() const { return m_last_error; }

private:
  struct param_t
  {
    u32 val;
    partype_t type;
    char* str;
  };

  enum segment_t
  {
    SEGMENT_CODE = 0,
    SEGMENT_DATA,
    SEGMENT_OVERLAY,
    SEGMENT_MAX
  };

  enum class OpcodeType
  {
    Primary,
    Extension
  };

  struct LocationContext
  {
    u32 line_num = 0;
    std::string line_text;
    std::optional<OpcodeType> opcode_type;
    std::optional<size_t> opcode_param_number;
    std::optional<std::string> included_file_path;
  };

  friend struct ::fmt::formatter<DSP::DSPAssembler::LocationContext>;

  // Utility functions
  s32 ParseValue(const char* str);
  u32 ParseExpression(const char* ptr);

  u32 GetParams(char* parstr, param_t* par);

  void InitPass(int pass);
  bool AssemblePass(const std::string& text, int pass);

  void ShowError(AssemblerError err_code);
  template <typename... Args>
  void ShowError(AssemblerError err_code, fmt::format_string<Args...> format, Args&&... args);
  template <typename... Args>
  void ShowWarning(fmt::format_string<Args...> format, Args&&... args);

  char* FindBrackets(char* src, char* dst);
  const DSPOPCTemplate* FindOpcode(std::string name, size_t par_count, OpcodeType type);
  bool VerifyParams(const DSPOPCTemplate* opc, param_t* par, size_t count, OpcodeType type);
  void BuildCode(const DSPOPCTemplate* opc, param_t* par, u32 par_count, u16* outbuf);

  std::vector<u16> m_output_buffer;

  std::string m_include_dir;

  u32 m_cur_addr = 0;
  int m_total_size = 0;
  u8 m_cur_pass = 0;

  LabelMap m_labels;

  bool m_failed = false;
  std::string m_last_error_str;
  AssemblerError m_last_error = AssemblerError::OK;

  using AliasMap = std::map<std::string, std::string>;
  AliasMap m_aliases;

  segment_t m_cur_segment = SEGMENT_CODE;
  u32 m_segment_addr[SEGMENT_MAX] = {};
  const AssemblerSettings m_settings;

  LocationContext m_location;
};
}  // namespace DSP
