// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>
#include <vector>
#include <wx/dialog.h>

class NetPlayServer;
class Player;
class wxCloseEvent;
class wxGauge;
class wxStaticText;
class wxWindow;

class MD5Dialog final : public wxDialog
{
public:
  MD5Dialog(wxWindow* parent, NetPlayServer* server, std::vector<const Player*> players,
            const std::string& game);

  void SetProgress(int pid, int progress);
  void SetResult(int pid, const std::string& result);

private:
  bool AllHashesMatch() const;
  void OnClose(wxCloseEvent& event);
  void OnCloseBtnPressed(wxCommandEvent& event);

  wxStaticText* m_final_result_label;
  NetPlayServer* m_netplay_server;
  std::map<int, wxGauge*> m_progress_bars;
  std::map<int, wxStaticText*> m_result_labels;
  std::vector<std::string> m_hashes;
};
