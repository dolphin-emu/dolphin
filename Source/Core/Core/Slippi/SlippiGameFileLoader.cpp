#include "SlippiGameFileLoader.h"

#include "Common/Logging/Log.h"
#include "Common/FileUtil.h"
#include "Common/File.h"
#include "Core/Boot/Boot.h"
#include "Core/Core.h"
#include "Core/ConfigManager.h"

std::string getFilePath(std::string fileName)
{
  std::string dirPath = File::GetSysDirectory();
  std::string filePath = dirPath + "GameFiles/GALE01/" + fileName; // TODO: Handle other games?

  if (File::Exists(filePath))
  {
    return filePath;
  }

  filePath = filePath + ".diff";
  if (File::Exists(filePath))
  {
    return filePath;
  }

  return "";
}

// SLIPPITODO: Revisit. Modified this function a bit, unsure of functionality
void ReadFileToBuffer(std::string& fileName, std::vector<u8>& buf)
{
  // Clear anything that was in the buffer
  buf.clear();

  // Don't do anything if a game is not running
  if (Core::GetState() != Core::State::Running)
    return;

  File::IOFile file(fileName, "rb");
  auto fileSize = file.GetSize();
  buf.resize(fileSize);
  size_t bytes_read;
  file.ReadArray<u8>(buf.data(), std::min<u64>(file.GetSize(), buf.size()), &bytes_read);  
}

u32 SlippiGameFileLoader::LoadFile(std::string fileName, std::string& data)
{
  if (fileCache.count(fileName))
  {
    data = fileCache[fileName];
    return (u32)data.size();
  }

  INFO_LOG(SLIPPI, "Loading file: %s", fileName.c_str());

  std::string gameFilePath = getFilePath(fileName);
  if (gameFilePath.empty())
  {
    fileCache[fileName] = "";
    data = "";
    return 0;
  }

  std::string fileContents;
  File::ReadFileToString(gameFilePath, fileContents);

  if (gameFilePath.substr(gameFilePath.length() - 5) == ".diff")
  {
    // If the file was a diff file, load the main file from ISO and apply patch
    std::vector<u8> buf;
    INFO_LOG(SLIPPI, "Will process diff");
    ReadFileToBuffer(fileName, buf);
    std::string diffContents = fileContents;

    decoder.Decode((char*)buf.data(), buf.size(), diffContents, &fileContents);
  }

  fileCache[fileName] = fileContents;
  data = fileCache[fileName];
  INFO_LOG(SLIPPI, "File size: %d", (u32)data.size());
  return (u32)data.size();
}
