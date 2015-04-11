// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <wx/dialog.h>

#include "Core/NetPlayProto.h"

class Player;
class wxChoice;

class PadMapDialog final : public wxDialog
{
public:
	PadMapDialog(wxWindow* parent, PadMapping map[], PadMapping wiimotemap[], std::vector<const Player*>& player_list);

private:
	void OnAdjust(wxCommandEvent& event);

	wxChoice* m_map_cbox[8];
	PadMapping* const m_mapping;
	PadMapping* const m_wiimapping;
	std::vector<const Player*>& m_player_list;
};
