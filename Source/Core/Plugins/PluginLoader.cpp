
#include "PluginLoader.h"
#include <Core/Boot/Boot.h>
#include <Core/ConfigManager.h>
#include <DiscIO\Filesystem.h>
#include <VideoCommon\OnScreenDisplay.enum.h>
#include <VideoCommon\OnScreenDisplay.h>
#include <exception>
#include <filesystem>
#include <string>
#include "Common/FileUtil.h"
#include "PluginInterface.h"

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
    OSD::AddMessageStack(stack);
  }
  virtual void AddMessage(const char* message, u32 ms, u32 argb, const char* messageStack,
                          bool preventDuplicate, float scale) override
  {
    OSD::AddMessage(message, ms, argb, messageStack == 0 ? "" : messageStack, preventDuplicate,
                    scale);
  }
  virtual void AddTypedMessage(OSD::MessageType type, char* message, u32 ms, u32 argb,
                               const char* messageStack, bool preventDuplicate,
                               float scale) override
  {
    OSD::AddTypedMessage(type, std::move(message), ms, argb, std::move(messageStack),
                         preventDuplicate, scale);
  }
  virtual const char* GetGameId() override
  {;
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
      HMODULE dllHandle = LoadLibrary(entry.path().c_str());
      DllHandles.push_back(dllHandle);

      auto interfaceGetter =
          (GetPluginInterface_Events)GetProcAddress(dllHandle, "GetPluginInterface_Events");
      if (interfaceGetter)
      {
        auto pluginInterface = interfaceGetter(apiInstance);
        if (pluginInterface != 0)
        {
          EventPlugins.push_back(pluginInterface);
        }
      }
    }
  }
}
void Cleanup()
{
  EventPlugins.clear();

  for (auto dllHandle : DllHandles)
  {
    FreeLibrary(dllHandle);
  }
  DllHandles.clear();

  if (apiInstance != 0)
  {
    delete apiInstance;
    apiInstance = 0;
  }
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
      // ignore invalid calls
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
