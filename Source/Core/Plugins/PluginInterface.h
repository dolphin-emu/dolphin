#pragma once
#include <iostream>
#include <VideoCommon\OnScreenDisplay.enum.h>
#include <Common/CommonTypes.h>
#include "picojson.h"

namespace Plugins
{
class IPluginInterface_Events
{
public:
  IPluginInterface_Events(){};
  virtual ~IPluginInterface_Events(){};
  virtual void OnGameLoad(const char* gamePath) = 0;
  virtual void OnGameClose(const char* gamePath) = 0;
  virtual void OnFileAccess(const char* path) = 0;
};

class IAPI
{
public:
  IAPI(){};
  virtual ~IAPI(){};
  virtual void AddOSDMessageStack(float x_offset, float y_offset, OSD::MessageStackDirection dir,
                                  bool centered, bool reversed, const char* name) = 0;
  virtual void AddMessage(const char* message, u32 ms, u32 argb, const char* messageStack,
                          bool preventDuplicate, float scale) = 0;
  virtual void AddTypedMessage(OSD::MessageType type, char* message, u32 ms, u32 argb,
                               const char* messageStack, bool preventDuplicate, float scale) = 0;
  virtual const char* GetGameId() = 0;
};


typedef IPluginInterface_Events*(__stdcall* GetPluginInterface_Events)(IAPI* api);
}  // namespace Plugins
