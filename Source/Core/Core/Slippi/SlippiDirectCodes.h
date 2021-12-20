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
    std::string connectCode = "";
    std::string lastPlayed = "";
    bool isFavorite = false;
  };

  SlippiDirectCodes(std::string fileName);
  ~SlippiDirectCodes();

  void ReadFile();
  void AddOrUpdateCode(std::string code);
  std::string get(int index);
  int length();
  void Sort(u8 sortByProperty = SlippiDirectCodes::SORT_BY_TIME);
  std::string Autocomplete(std::string startText);

protected:
  void WriteFile();
  std::string getCodesFilePath();
  std::vector<CodeInfo> parseFile(std::string fileContents);
  std::vector<CodeInfo> directCodeInfos;
  std::string m_fileName;
};
