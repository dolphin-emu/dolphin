// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wx/dialog.h>

class GameListCtrl;
class wxListBox;

class ChangeGameDialog final : public wxDialog
{
public:
  ChangeGameDialog(wxWindow* parent, const GameListCtrl* const game_list);

  wxString GetChosenGameName() const;

private:
  void OnPick(wxCommandEvent& event);

  wxListBox* m_game_lbox;
  wxString m_game_name;
};
