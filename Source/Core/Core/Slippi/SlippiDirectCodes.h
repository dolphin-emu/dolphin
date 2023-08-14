#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>
#include "Common/CommonTypes.h"

class SlippiDirectCodes
{
public:
  static const uint8_t SORT_BY_TIME = 1;
  static const uint8_t SORT_BY_FAVORITE = 2;
  static const uint8_t SORT_BY_NAME = 3;

  struct CodeInfo
  {
    std::string connect_code = "";
    std::string last_played = "";
    bool is_favorite = false;
  };

  SlippiDirectCodes(std::string file_name);
  ~SlippiDirectCodes();

  void ReadFile();
  void AddOrUpdateCode(std::string code);
  std::string get(int index);
  int length();
  void Sort(u8 sort_by_property = SlippiDirectCodes::SORT_BY_TIME);
  std::string Autocomplete(std::string start_text);

protected:
  void WriteFile();
  std::string getCodesFilePath();
  std::vector<CodeInfo> parseFile(std::string file_contents);
  std::vector<CodeInfo> m_direct_code_infos;
  std::string m_file_name;
};
