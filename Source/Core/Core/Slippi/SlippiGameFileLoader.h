#pragma once

#include <open-vcdiff/src/google/vcdecoder.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <set>

#include "Common/CommonTypes.h"
#include "Core/System.h"

class SlippiGameFileLoader
{
public:
  u32 LoadFile(Core::System& system, std::string file_name, std::string& contents);

  // static stadium transformation files
  std::set<std::string> grpsx_strings = {"GrPs1.dat", "GrPs2.dat", "GrPs3.dat", "GrPs4.dat"};

protected:
  std::unordered_map<std::string, std::string> file_cache;
  open_vcdiff::VCDiffDecoder decoder;
};
