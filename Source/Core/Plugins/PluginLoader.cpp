
#include "PluginLoader.h"
#include <Core/Boot/Boot.h>
#include <Core/ConfigManager.h>
#include <DiscIO\Filesystem.h>
#include <VideoCommon\OnScreenDisplay.enum.h>
#include <VideoCommon\OnScreenDisplay.h>
#include <filesystem>
#include <string>
#include "PluginInterface.h"
#include "Common/FileUtil.h"

// TODO multiplatform
#include <windows.h>

namespace Plugins
{
std::vector<IPluginInterface_Events*> EventPlugins;
std::vector<HMODULE> DllHandles;


class API : public IAPI
{
public:
  const char* gameId;
  API() : IAPI() { gameId = SConfig::GetInstance().GetGameID().c_str(); }

  virtual void AddOSDMessageStack(float x_offset, float y_offset, OSD::MessageStackDirection dir,
                          bool centered, bool reversed, const char* name) override
  {
    auto stack = OSD::OSDMessageStack(x_offset, y_offset, dir, centered, reversed, std::move(name));
    //auto stack = OSD::OSDMessageStack(0, 0, OSD::MessageStackDirection::Upward, true, true, "R79JAF_subtitles");
    OSD::AddMessageStack(stack);
  }
  virtual void AddMessage(const char* message, u32 ms, u32 argb, const char* messageStack,
                  bool preventDuplicate) override
  {
    OSD::AddMessage(message, ms, argb, messageStack == 0 ? "" : messageStack, preventDuplicate);
    //OSD::AddMessage(message, ms, argb, "R79JAF_subtitles", false);
  }
  virtual void AddTypedMessage(OSD::MessageType type, char* message, u32 ms, u32 argb,
                       const char* messageStack, bool preventDuplicate) override
  {
    //NOTICE_LOG_FMT(BOOT, "Calling {}", "AddTypedMessage");
    OSD::AddTypedMessage(type, std::move(message), ms, argb, std::move(messageStack),
                         preventDuplicate);
  }
  virtual const char* GetGameId() override
  {
    //NOTICE_LOG_FMT(BOOT, "Calling {}", "GetGameId");
    return gameId;
  }
};
API* apiInstance = 0;

// TODO get rom path insteead of getting disc as param
void Init()
{
  Cleanup();

  apiInstance = new API();

  auto dlldir = File::GetUserPath(D_PLUGINS_IDX);

  if (!std::filesystem::exists(dlldir))
  {
    return;
  }
  for (const auto& entry : std::filesystem::directory_iterator(dlldir))
  {
    if (entry.path().extension() == ".dll")
    {
      OSD::AddMessage("Loading plugin library " + entry.path().string());
      HMODULE dllHandle = LoadLibrary(entry.path().c_str());

      auto interfaceGetter =
          (GetPluginInterface_Events)GetProcAddress(dllHandle, "GetPluginInterface_Events");
      if (interfaceGetter)
      {
        //OSD::AddMessage("found dll method");
        auto pluginInterface = interfaceGetter(apiInstance);
        if (pluginInterface != 0)
        {
          //OSD::AddMessage("storing plugin interface");
          EventPlugins.push_back(pluginInterface);
        }
      }
    }
  }
}
void Cleanup()
{
  if (apiInstance != 0)
  {
    delete apiInstance;
  }

  EventPlugins.clear();

  for (auto dllHandle : DllHandles)
  {
    FreeLibrary(dllHandle);
  }
  DllHandles.clear();
}

void OnGameLoad(const char* path)
{
  for (auto i : EventPlugins)
  {
    i->OnGameLoad(path);
  }
}
void OnGameClose(const char* path)
{
  for (auto i : EventPlugins)
  {
    i->OnGameClose(path);
  }
}

void OnFileAccess(const DiscIO::Volume& volume, const DiscIO::Partition& partition, u64 offset)
{
  if (!EventPlugins.empty())
  {
    const DiscIO::FileSystem* file_system = volume.GetFileSystem(partition);
    if (!file_system)
    {
      //ignore invalid calls
      return;
    }

    const std::unique_ptr<DiscIO::FileInfo> file_info = file_system->FindFileInfo(offset);
    const char* path = file_info->GetPath().c_str();

    for (auto i : EventPlugins)
    {
      i->OnFileAccess(path);
    }
  }
}
}  // namespace Plugins
