#include "Common/Assembler/AssemblerShared.h"

#include <fmt/format.h>

namespace Common::GekkoAssembler
{
std::string AssemblerError::FormatError() const
{
  char space_char = ' ';
  if (col == 0)
  {
    space_char = '\0';
  }

  std::string_view line_str = error_line;
  if (line_str.back() == '\n')
  {
    line_str = line_str.substr(0, line_str.length() - 1);
  }

  return fmt::format("Error on line {0} col {1}:\n"
                     "  {2}\n"
                     "  {3:{4}}{5:^^{6}}\n"
                     "{7}",
                     line + 1, col + 1, line_str, space_char, col, '^', len, message);
}
}  // namespace Common::GekkoAssembler
