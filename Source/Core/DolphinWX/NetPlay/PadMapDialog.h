// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <wx/dialog.h>

#include "Core/NetPlayProto.h"

class NetPlayClient;
class NetPlayServer;
class Player;
class wxChoice;

class PadMapDialog final : public wxDialog
{
public:
	PadMapDialog(wxWindow* parent, NetPlayServer* server, NetPlayClient* client);

	PadMappingArray GetModifiedPadMappings() const;

private:
	void OnAdjust(wxCommandEvent& event);

	wxChoice* m_map_cbox[4];
	PadMappingArray m_pad_mapping;
	std::vector<const Player*> m_player_list;
};
