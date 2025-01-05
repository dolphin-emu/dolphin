#include "SlippiGameFileLoader.h"

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Core/Boot/Boot.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDThread.h"
#include "Core/System.h"

std::string getFilePath(std::string file_name)
{
  std::string dir_path = File::GetSysDirectory();

  std::string file_path = dir_path + "GameFiles/GALE01/" + file_name;  // TODO: Handle other games?

  if (File::Exists(file_path))
  {
    return file_path;
  }

  file_path = file_path + ".diff";
  if (File::Exists(file_path))
  {
    return file_path;
  }

  return "";
}

u32 SlippiGameFileLoader::LoadFile(std::string file_name, std::string& data)
{
  if (file_cache.count(file_name))
  {
    data = file_cache[file_name];
    return static_cast<u32>(data.size());
  }

  INFO_LOG_FMT(SLIPPI, "Loading file: {}", file_name.c_str());

  std::string game_file_path = getFilePath(file_name);
  if (game_file_path.empty())
  {
    file_cache[file_name] = "";
    data = "";
    return 0;
  }

  std::string file_contents;
  File::ReadFileToString(game_file_path, file_contents);

  // If the file was a diff file and the game is running, load the main file from ISO and apply
  // patch
  if (game_file_path.substr(game_file_path.length() - 5) == ".diff" &&
      Core::GetState() == Core::State::Running)
  {
    std::vector<u8> buf;
    INFO_LOG_FMT(SLIPPI, "Will process diff");
    Core::System::GetInstance().GetDVDThread().ReadFile(file_name, buf);
    std::string diff_contents = file_contents;
    decoder.Decode((char*)buf.data(), buf.size(), diff_contents, &file_contents);
  }

  file_cache[file_name] = file_contents;
  data = file_cache[file_name];
  INFO_LOG_FMT(SLIPPI, "File size: {}", static_cast<u32>(data.size()));
  return static_cast<u32>(data.size());
}
