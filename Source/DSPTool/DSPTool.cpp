// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"
#include "Core/DSP/DSPCodeUtil.h"
#include "Core/DSP/DSPDisassembler.h"
#include "Core/DSP/DSPHost.h"
#include "Core/DSP/DSPTables.h"

// Stub out the dsplib host stuff, since this is just a simple cmdline tools.
u8 DSP::Host::ReadHostMemory(u32 addr)
{
  return 0;
}
void DSP::Host::WriteHostMemory(u8 value, u32 addr)
{
}
void DSP::Host::OSD_AddMessage(const std::string& str, u32 ms)
{
}
bool DSP::Host::OnThread()
{
  return false;
}
bool DSP::Host::IsWiiHost()
{
  return false;
}
void DSP::Host::CodeLoaded(const u8* ptr, int size)
{
}
void DSP::Host::InterruptRequest()
{
}
void DSP::Host::UpdateDebugger()
{
}

static std::string CodeToHeader(const std::vector<u16>& code, const std::string& filename)
{
  std::vector<u16> code_padded = code;

  // Pad with nops to 32byte boundary
  while (code_padded.size() & 0x7f)
    code_padded.push_back(0);

  std::string header;
  header.reserve(code_padded.size() * 4);
  header.append("#define NUM_UCODES 1\n\n");
  std::string filename_without_extension;
  SplitPath(filename, nullptr, &filename_without_extension, nullptr);
  header.append(StringFromFormat("const char* UCODE_NAMES[NUM_UCODES] = {\"%s\"};\n\n",
                                 filename_without_extension.c_str()));
  header.append("const unsigned short dsp_code[NUM_UCODES][0x1000] = {\n");

  header.append("\t{\n\t\t");
  for (u32 j = 0; j < code_padded.size(); j++)
  {
    if (j && ((j & 15) == 0))
      header.append("\n\t\t");
    header.append(StringFromFormat("0x%04x, ", code_padded[j]));
  }
  header.append("\n\t},\n");

  header.append("};\n");
  return header;
}

static std::string CodesToHeader(const std::vector<std::vector<u16>>& codes,
                                 const std::vector<std::string>& filenames)
{
  std::vector<std::vector<u16>> codes_padded;
  std::size_t reserve_size = 0;
  for (std::size_t i = 0; i < codes.size(); i++)
  {
    codes_padded.push_back(codes[i]);
    // Pad with nops to 32byte boundary
    while (codes_padded[i].size() & 0x7f)
      codes_padded[i].push_back(0);

    reserve_size += codes_padded[i].size();
  }

  std::string header;
  header.reserve(reserve_size * 4);
  header.append(StringFromFormat("#define NUM_UCODES %zu\n\n", codes.size()));
  header.append("const char* UCODE_NAMES[NUM_UCODES] = {\n");
  for (const std::string& in_filename : filenames)
  {
    std::string filename;
    if (!SplitPath(in_filename, nullptr, &filename, nullptr))
      filename = in_filename;
    header.append(StringFromFormat("\t\"%s\",\n", filename.c_str()));
  }
  header.append("};\n\n");
  header.append("const unsigned short dsp_code[NUM_UCODES][0x1000] = {\n");

  for (std::size_t i = 0; i < codes.size(); i++)
  {
    if (codes[i].empty())
      continue;

    header.append("\t{\n\t\t");
    for (std::size_t j = 0; j < codes_padded[i].size(); j++)
    {
      if (j && ((j & 15) == 0))
        header.append("\n\t\t");
      header.append(StringFromFormat("0x%04x, ", codes_padded[i][j]));
    }
    header.append("\n\t},\n");
  }
  header.append("};\n");
  return header;
}

static void PerformBinaryComparison(const std::string& lhs, const std::string& rhs)
{
  std::string binary_code;

  File::ReadFileToString(lhs, binary_code);
  const std::vector<u16> code1 = DSP::BinaryStringBEToCode(binary_code);

  File::ReadFileToString(rhs, binary_code);
  const std::vector<u16> code2 = DSP::BinaryStringBEToCode(binary_code);

  DSP::Compare(code1, code2);
}

static void PrintResults(const std::string& input_name, const std::string& output_name,
                         bool print_results_srhack, bool print_results_prodhack)
{
  std::string dumpfile;

  File::ReadFileToString(input_name, dumpfile);
  const std::vector<u16> reg_vector = DSP::BinaryStringBEToCode(dumpfile);

  std::string results("Start:\n");
  for (int initial_reg = 0; initial_reg < 32; initial_reg++)
  {
    results.append(StringFromFormat("%02x %04x ", initial_reg, reg_vector.at(initial_reg)));
    if ((initial_reg + 1) % 8 == 0)
      results.append("\n");
  }

  results.append("\n");
  results.append("Step [number]:\n[Reg] [last value] [current value]\n\n");

  for (unsigned int step = 1; step < reg_vector.size() / 32; step++)
  {
    bool changed = false;
    u16 current_reg;
    u16 last_reg;
    u32 htemp;
    // results.append(StringFromFormat("Step %3d: (CW 0x%04x) UC:%03d\n", step, 0x8fff+step,
    // (step-1)/32));
    results.append(StringFromFormat("Step %3d:\n", step));
    for (int reg = 0; reg < 32; reg++)
    {
      if (reg >= 0x0c && reg <= 0x0f)
        continue;
      if (print_results_srhack && reg == 0x13)
        continue;

      if (print_results_prodhack && reg >= 0x15 && reg <= 0x17)
      {
        switch (reg)
        {
        case 0x15:  // DSP_REG_PRODM
          last_reg =
              reg_vector.at((step * 32 - 32) + reg) + reg_vector.at((step * 32 - 32) + reg + 2);
          current_reg = reg_vector.at(step * 32 + reg) + reg_vector.at(step * 32 + reg + 2);
          break;
        case 0x16:  // DSP_REG_PRODH
          htemp = ((reg_vector.at(step * 32 + reg - 1) + reg_vector.at(step * 32 + reg + 1)) &
                   ~0xffff) >>
                  16;
          current_reg = (u8)(reg_vector.at(step * 32 + reg) + htemp);
          htemp =
              ((reg_vector.at(step * 32 - 32 + reg - 1) + reg_vector.at(step * 32 - 32 + reg + 1)) &
               ~0xffff) >>
              16;
          last_reg = (u8)(reg_vector.at(step * 32 - 32 + reg) + htemp);
          break;
        case 0x17:  // DSP_REG_PRODM2
        default:
          current_reg = 0;
          last_reg = 0;
          break;
        }
      }
      else
      {
        current_reg = reg_vector.at(step * 32 + reg);
        last_reg = reg_vector.at((step * 32 - 32) + reg);
      }
      if (last_reg != current_reg)
      {
        results.append(StringFromFormat("%02x %-7s: %04x %04x\n", reg, DSP::pdregname(reg),
                                        last_reg, current_reg));
        changed = true;
      }
    }

    if (changed)
      results.append("\n");
    else
      results.append("No Change\n\n");
  }

  if (output_name.empty())
    printf("%s", results.c_str());
  else
    File::WriteStringToFile(output_name, results);
}

static bool PerformDisassembly(const std::string& input_name, const std::string& output_name)
{
  if (input_name.empty())
  {
    printf("Disassemble: Must specify input.\n");
    return false;
  }

  std::string binary_code;
  File::ReadFileToString(input_name, binary_code);
  const std::vector<u16> code = DSP::BinaryStringBEToCode(binary_code);
  std::string text;
  DSP::Disassemble(code, true, text);

  if (output_name.empty())
    printf("%s", text.c_str());
  else
    File::WriteStringToFile(output_name, text);

  printf("Disassembly completed successfully!\n");
  return true;
}

static std::vector<std::string> GetAssemblerFiles(const std::string& source)
{
  std::vector<std::string> files;
  std::size_t last_pos = 0;
  std::size_t pos = 0;

  while ((pos = source.find('\n', last_pos)) != std::string::npos)
  {
    std::string temp = source.substr(last_pos, pos - last_pos);
    if (!temp.empty())
      files.push_back(std::move(temp));
    last_pos = pos + 1;
  }

  return files;
}

static bool PerformAssembly(const std::string& input_name, const std::string& output_name,
                            const std::string& output_header_name, bool multiple, bool force,
                            bool output_size)
{
  if (input_name.empty())
  {
    printf("Assemble: Must specify input.\n");
    return false;
  }

  std::string source;
  if (File::ReadFileToString(input_name, source))
  {
    if (multiple)
    {
      source.append("\n");

      // When specifying a list of files we must compile a header
      // (we can't assemble multiple files to one binary)
      // since we checked it before, we assume output_header_name isn't empty
      std::string currentSource;
      const std::vector<std::string> files = GetAssemblerFiles(source);

      std::size_t lines = files.size();
      if (lines == 0)
      {
        printf("ERROR: Must specify at least one file\n");
        return false;
      }

      std::vector<std::vector<u16>> codes(lines);

      for (std::size_t i = 0; i < lines; i++)
      {
        if (!File::ReadFileToString(files[i], currentSource))
        {
          printf("ERROR reading %s, skipping...\n", files[i].c_str());
          lines--;
        }
        else
        {
          if (!DSP::Assemble(currentSource, codes[i], force))
          {
            printf("Assemble: Assembly of %s failed due to errors\n", files[i].c_str());
            lines--;
          }
          if (output_size)
          {
            printf("%s: %zu\n", files[i].c_str(), codes[i].size());
          }
        }
      }

      const std::string header = CodesToHeader(codes, files);
      File::WriteStringToFile(output_header_name + ".h", header);
    }
    else
    {
      std::vector<u16> code;

      if (!DSP::Assemble(source, code, force))
      {
        printf("Assemble: Assembly failed due to errors\n");
        return false;
      }

      if (output_size)
      {
        printf("%s: %zu\n", input_name.c_str(), code.size());
      }

      if (!output_name.empty())
      {
        const std::string binary_code = DSP::CodeToBinaryStringBE(code);
        File::WriteStringToFile(output_name, binary_code);
      }
      if (!output_header_name.empty())
      {
        const std::string header = CodeToHeader(code, input_name);
        File::WriteStringToFile(output_header_name + ".h", header);
      }
    }
  }

  source.clear();

  if (!output_size)
    printf("Assembly completed successfully!\n");

  return true;
}

static bool IsHelpFlag(const std::string& argument)
{
  return argument == "--help" || argument == "-?";
}

// Usage:
// Disassemble a file:
//   dsptool -d -o asdf.txt asdf.bin
// Disassemble a file, output to standard output:
//   dsptool -d asdf.bin
// Assemble a file:
//   dsptool [-f] -o asdf.bin asdf.txt
// Assemble a file, output header:
//   dsptool [-f] -h asdf.h asdf.txt
// Print results from DSPSpy register dump
//   dsptool -p dsp_dump0.bin
int main(int argc, const char* argv[])
{
  if (argc == 1 || (argc == 2 && IsHelpFlag(argv[1])))
  {
    printf("USAGE: DSPTool [-?] [--help] [-f] [-d] [-m] [-p <FILE>] [-o <FILE>] [-h <FILE>] <DSP "
           "ASSEMBLER FILE>\n");
    printf("-? / --help: Prints this message\n");
    printf("-d: Disassemble\n");
    printf("-m: Input file contains a list of files (Header assembly only)\n");
    printf("-s: Print the final size in bytes (only)\n");
    printf("-f: Force assembly (errors are not critical)\n");
    printf("-o <OUTPUT FILE>: Results from stdout redirected to a file\n");
    printf("-h <HEADER FILE>: Output assembly results to a header\n");
    printf("-p <DUMP FILE>: Print results of DSPSpy register dump\n");
    printf("-ps <DUMP FILE>: Print results of DSPSpy register dump (disable SR output)\n");
    printf("-pm <DUMP FILE>: Print results of DSPSpy register dump (convert PROD values)\n");
    printf("-psm <DUMP FILE>: Print results of DSPSpy register dump (convert PROD values/disable "
           "SR output)\n");

    return 0;
  }

  std::string input_name;
  std::string output_header_name;
  std::string output_name;

  bool disassemble = false, compare = false, multiple = false, outputSize = false, force = false,
       print_results = false, print_results_prodhack = false, print_results_srhack = false;
  for (int i = 1; i < argc; i++)
  {
    const std::string argument = argv[i];
    if (argument == "-d")
      disassemble = true;
    else if (argument == "-o")
      output_name = argv[++i];
    else if (argument == "-h")
      output_header_name = argv[++i];
    else if (argument == "-c")
      compare = true;
    else if (argument == "-s")
      outputSize = true;
    else if (argument == "-m")
      multiple = true;
    else if (argument == "-f")
      force = true;
    else if (argument == "-p")
      print_results = true;
    else if (argument == "-ps")
    {
      print_results = true;
      print_results_srhack = true;
    }
    else if (argument == "-pm")
    {
      print_results = true;
      print_results_prodhack = true;
    }
    else if (argument == "-psm")
    {
      print_results = true;
      print_results_srhack = true;
      print_results_prodhack = true;
    }
    else
    {
      if (!input_name.empty())
      {
        printf("ERROR: Can only take one input file.\n");
        return 1;
      }
      input_name = argv[i];
      if (!File::Exists(input_name))
      {
        printf("ERROR: Input path does not exist.\n");
        return 1;
      }
    }
  }

  if (multiple && (compare || disassemble || !output_name.empty() || input_name.empty()))
  {
    printf("ERROR: Multiple files can only be used with assembly "
           "and must compile a header file.\n");
    return 1;
  }

  if (compare)
  {
    PerformBinaryComparison(input_name, output_name);
    return 0;
  }

  if (print_results)
  {
    PrintResults(input_name, output_name, print_results_srhack, print_results_prodhack);
    return 0;
  }

  if (disassemble)
  {
    if (!PerformDisassembly(input_name, output_name))
      return 1;
  }
  else
  {
    if (!PerformAssembly(input_name, output_name, output_header_name, multiple, force, outputSize))
      return 1;
  }

  return 0;
}
