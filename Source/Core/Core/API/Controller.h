// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <experimental/map> // std::erase_if is standardized in C++20, but not C++17 yet

#include "Common/CommonTypes.h"
#include "Core/API/Events.h"
#include "Core/Movie.h"
#include "InputCommon/GCPadStatus.h"

namespace API
{
enum class ClearOn
{
  NextPoll = 0,
  NextFrame = 1,
  NextOverride = 2,
};

template <typename T>
class BaseManip
{
public:
  BaseManip(API::EventHub& event_hub) : m_event_hub(event_hub)
  {
    m_frame_advanced_listener = m_event_hub.ListenEvent<API::Events::FrameAdvance>(
        [&](const API::Events::FrameAdvance&) { NotifyFrameAdvanced(); });
  }
  ~BaseManip() { m_event_hub.UnlistenEvent(m_frame_advanced_listener); }
  void Clear() { m_overrides.clear(); }
  void NotifyFrameAdvanced()
  {
    std::experimental::erase_if(m_overrides, [](const auto& kvp) {
      return kvp.second.clear_on == ClearOn::NextFrame && kvp.second.used;
    });
  }

protected:
  std::map<int, T> m_overrides;

private:
  API::EventHub& m_event_hub;
  API::ListenerID<API::Events::FrameAdvance> m_frame_advanced_listener;
};

/*
struct WiiInputOverride
{
  // WiimoteCommon::DataReportBuilder rpt; ???
  // int ext; ???
  // WiimoteEmu::EncryptionKey& key; ???
  ClearOn clear_on;
  bool used;
};

class WiiManip : public BaseManip<WiiInputOverride>
{
public:
  using BaseManip::BaseManip;
  //GCPadStatus Get(int controller_id); ???
  //void Set(GCPadStatus pad_status, int controller_id, ClearOn clear_on); ???
  void PerformInputManip(WiimoteCommon::DataReportBuilder& rpt, int controller_id, int ext,
                         const WiimoteEmu::EncryptionKey& key);
};
*/

struct GCInputOverride
{
  GCPadStatus pad_status;
  ClearOn clear_on;
  bool used;
};

class GCManip : public BaseManip<GCInputOverride>
{
public:
  using BaseManip::BaseManip;
  GCPadStatus Get(int controller_id);
  void Set(GCPadStatus pad_status, int controller_id, ClearOn clear_on);
  void PerformInputManip(GCPadStatus* pad_status, int controller_id);
};

// global instances
GCManip& GetGCManip();
//WiiManip& GetWiiManip();

}  // namespace API
