#pragma once

#include <string>
#include "Core/Movie.h"
#include <Core/NetPlayProto.h>
#include "Core/HW/SI/SI_Device.h"
#include "Core/ConfigManager.h"

class StateAuxillary
{
public:
  static void saveState(const std::string& filename, bool wait = false);
  static void startRecording();
  static void stopRecording(const std::string replay_path, tm* matchDateTimeParam);
  static void endPlayback();
  static void setNetPlayControllers(NetPlay::PadMappingArray m_pad_map, NetPlay::PlayerId m_pid);
  static int getOurNetPlayPort();
  static std::vector<int> getOurNetPlayPorts();
  static bool isSpectator();
  static void setPrePort(SerialInterface::SIDevices currentPort0,
                         SerialInterface::SIDevices currentPort1,
                         SerialInterface::SIDevices currentPort2,
                         SerialInterface::SIDevices currentPort3);
  static void setPostPort();
  static bool getBoolMatchStart();
  static bool getBoolMatchEnd();
  static bool getBoolWroteCodes();
  static void setBoolMatchStart(bool boolValue);
  static void setBoolMatchEnd(bool boolValue);
  static void setBoolWroteCodes(bool boolValue);
};
