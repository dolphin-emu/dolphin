#pragma once

#include <open-vcdiff/src/google/vcdecoder.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "Common/CommonTypes.h"

class SlippiGameFileLoader
{
public:
  u32 LoadFile(std::string file_name, std::string& contents);

protected:
  std::unordered_map<std::string, std::string> file_cache;
  open_vcdiff::VCDiffDecoder decoder;
};
