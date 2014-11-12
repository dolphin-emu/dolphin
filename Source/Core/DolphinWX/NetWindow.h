// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/frame.h>

#include "Common/FifoQueue.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"

class CGameListCtrl;
class wxButton;
class wxCheckBox;
class wxChoice;
class wxListBox;
class wxString;
class wxTextCtrl;
class wxWindow;

enum
{
	NP_GUI_EVT_CHANGE_GAME = 45,
	NP_GUI_EVT_START_GAME,
	NP_GUI_EVT_STOP_GAME,
};

class NetPlaySetupDiag : public wxFrame
{
public:
	NetPlaySetupDiag(wxWindow* const parent, const CGameListCtrl* const game_list);
	~NetPlaySetupDiag();
private:
	void OnJoin(wxCommandEvent& event);
	void OnHost(wxCommandEvent& event);
	void OnQuit(wxCommandEvent& event);

	void MakeNetPlayDiag(int port, const std::string &game, bool is_hosting);

	wxTextCtrl* m_nickname_text;
	wxTextCtrl* m_host_port_text;
	wxTextCtrl* m_connect_port_text;
	wxTextCtrl* m_connect_ip_text;

	wxListBox*  m_game_lbox;
#ifdef USE_UPNP
	wxCheckBox* m_upnp_chk;
#endif

	const CGameListCtrl* const m_game_list;
};

class NetPlayDiag : public wxFrame, public NetPlayUI
{
public:
	NetPlayDiag(wxWindow* const parent, const CGameListCtrl* const game_list
		, const std::string& game, const bool is_hosting = false);
	~NetPlayDiag();

	Common::FifoQueue<std::string> chat_msgs;

	void OnStart(wxCommandEvent& event);

	// implementation of NetPlayUI methods
	void BootGame(const std::string& filename) override;
	void StopGame() override;

	void Update() override;
	void AppendChat(const std::string& msg) override;

	void OnMsgChangeGame(const std::string& filename) override;
	void OnMsgStartGame() override;
	void OnMsgStopGame() override;

	static NetPlayDiag *&GetInstance() { return npd; };

	bool IsRecording() override;

private:
	void OnChat(wxCommandEvent& event);
	void OnQuit(wxCommandEvent& event);
	void OnThread(wxThreadEvent& event);
	void OnChangeGame(wxCommandEvent& event);
	void OnAdjustBuffer(wxCommandEvent& event);
	void OnConfigPads(wxCommandEvent& event);
	void OnKick(wxCommandEvent& event);
	void OnPlayerSelect(wxCommandEvent& event);
	void GetNetSettings(NetSettings &settings);
	std::string FindGame();

	wxListBox*   m_player_lbox;
	wxTextCtrl*  m_chat_text;
	wxTextCtrl*  m_chat_msg_text;
	wxCheckBox*  m_memcard_write;
	wxCheckBox*  m_record_chkbox;

	std::string  m_selected_game;
	wxButton*    m_game_btn;
	wxButton*    m_start_btn;
	wxButton*    m_kick_btn;

	std::vector<int> m_playerids;

	const CGameListCtrl* const m_game_list;

	static NetPlayDiag* npd;
};

class ChangeGameDiag : public wxDialog
{
public:
	ChangeGameDiag(wxWindow* const parent, const CGameListCtrl* const game_list, wxString& game_name);

private:
	void OnPick(wxCommandEvent& event);

	wxListBox* m_game_lbox;
	wxString&  m_game_name;
};

class PadMapDiag : public wxDialog
{
public:
	PadMapDiag(wxWindow* const parent, PadMapping map[], PadMapping wiimotemap[], std::vector<const Player *>& player_list);

private:
	void OnAdjust(wxCommandEvent& event);

	wxChoice* m_map_cbox[8];
	PadMapping* const m_mapping;
	PadMapping* const m_wiimapping;
	std::vector<const Player *>& m_player_list;
};

namespace NetPlay
{
	void StopGame();
}
