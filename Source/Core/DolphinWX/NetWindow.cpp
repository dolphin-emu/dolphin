// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>
#include <wx/anybutton.h>
#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/chartype.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/defs.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/frame.h>
#include <wx/gdicmn.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/translation.h>


#include "Common/CommonTypes.h"
#include "Common/FifoQueue.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"

#include "Core/ConfigManager.h"
#include "Core/CoreParameter.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayProto.h"
#include "Core/NetPlayServer.h"
#include "Core/HW/EXI_Device.h"

#include "DolphinWX/Frame.h"
#include "DolphinWX/GameListCtrl.h"
#include "DolphinWX/ISOFile.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/NetWindow.h"
#include "DolphinWX/WxUtils.h"

#define INITIAL_PAD_BUFFER_SIZE 5

static NetPlayServer* netplay_server = nullptr;
static NetPlayClient* netplay_client = nullptr;
NetPlayDiag *NetPlayDiag::npd = nullptr;


static wxString FailureReasonStringForHostLabel(int reason)
{
	switch (reason)
	{
	case TraversalClient::BadHost:
		return _("(Error: Bad host)");
	case TraversalClient::VersionTooOld:
		return _("(Error: Dolphin too old)");
	case TraversalClient::ServerForgotAboutUs:
		return _("(Error: Disconnected)");
	case TraversalClient::SocketSendError:
		return _("(Error: Socket)");
	case TraversalClient::ResendTimeout:
		return _("(Error: Timeout)");
	default:
		return _("(Error: Unknown)");
	}
}

static std::string BuildGameName(const GameListItem& game)
{
	// Lang needs to be consistent
	auto const lang = 0;

	std::string name(game.GetName(lang));

	if (game.GetRevision() != 0)
		return name + " (" + game.GetUniqueID() + ", Revision " + std::to_string((long long)game.GetRevision()) + ")";
	else
		return name + " (" + game.GetUniqueID() + ")";
}

static void FillWithGameNames(wxListBox* game_lbox, const CGameListCtrl& game_list)
{
	for (u32 i = 0; auto game = game_list.GetISO(i); ++i)
		game_lbox->Append(StrToWxStr(BuildGameName(*game)));
}

NetPlaySetupDiag::NetPlaySetupDiag(wxWindow* const parent, const CGameListCtrl* const game_list)
	: wxFrame(parent, wxID_ANY, _("Dolphin NetPlay Setup"))
	, m_game_list(game_list)
{
	IniFile inifile;
	inifile.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	wxPanel* const panel = new wxPanel(this);

	// top row
	wxBoxSizer* const trav_szr = new wxBoxSizer(wxHORIZONTAL);

	m_direct_traversal = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxSize(75, -1));
	m_direct_traversal->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &NetPlaySetupDiag::OnChoice, this);
	m_direct_traversal->Append(_("Direct"));
	m_direct_traversal->Append(_("Traversal"));

	std::string travChoice;
	netplay_section.Get("TraversalChoice", &travChoice, "direct");

	if (travChoice == "traversal")
	{
		m_direct_traversal->Select(1);
	}
	else
	{
		m_direct_traversal->Select(0);
	}

	trav_szr->Add(m_direct_traversal, 0, wxRIGHT);

	wxBoxSizer* const nick_szr = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText* const nick_lbl = new wxStaticText(panel, wxID_ANY, _("Nickname :"));
	std::string nickname;
	netplay_section.Get("Nickname", &nickname, "Player");
	m_nickname_text = new wxTextCtrl(panel, wxID_ANY, StrToWxStr(nickname));
	nick_szr->Add(nick_lbl, 0, wxCENTER);
	nick_szr->Add(m_nickname_text, 0, wxALL, 5);

	std::string centralServer;
	netplay_section.Get("TraversalServer", &centralServer, "");
	m_traversal_server_lbl = new wxStaticText(panel, wxID_ANY, _("Traversal:"));
	m_traversal_server = new wxTextCtrl(panel, wxID_ANY, StrToWxStr(centralServer));
	nick_szr->Add(m_traversal_server_lbl, 0, wxCENTER);
	nick_szr->Add(m_traversal_server, 0, wxALL, 5);

	std::string centralPort;
	netplay_section.Get("TraversalPort", &centralPort, "");
	m_traversal_port_lbl = new wxStaticText(panel, wxID_ANY, _("Port:"));
	m_traversal_port = new wxTextCtrl(panel, wxID_ANY, StrToWxStr(centralPort));
	nick_szr->Add(m_traversal_port_lbl, 0, wxCENTER);
	nick_szr->Add(m_traversal_port, 0, wxALL, 5);

	// tabs
	wxNotebook* const notebook = new wxNotebook(panel, wxID_ANY);
	wxPanel* const connect_tab = new wxPanel(notebook, wxID_ANY);
	notebook->AddPage(connect_tab, _("Connect"));
	wxPanel* const host_tab = new wxPanel(notebook, wxID_ANY);
	notebook->AddPage(host_tab, _("Host"));

	// connect tab
	{
		m_ip_lbl = new wxStaticText(connect_tab, wxID_ANY, _("Host Code :"));

		std::string address;
		netplay_section.Get("HostCode", &address, "00000000");
		m_connect_ip_text = new wxTextCtrl(connect_tab, wxID_ANY, StrToWxStr(address));

		m_client_port_lbl = new wxStaticText(connect_tab, wxID_ANY, _("Port :"));

		// string? w/e
		std::string port;
		netplay_section.Get("ConnectPort", &port, "2626");
		m_connect_port_text = new wxTextCtrl(connect_tab, wxID_ANY, StrToWxStr(port));

		wxButton* const connect_btn = new wxButton(connect_tab, wxID_ANY, _("Connect"));
		connect_btn->Bind(wxEVT_BUTTON, &NetPlaySetupDiag::OnJoin, this);

		wxStaticText* const alert_lbl = new wxStaticText(connect_tab, wxID_ANY,
			_("ALERT:\n\n"
			"Netplay will only work with the following settings:\n"
			" - DSP Emulator Engine Must be the same on all computers!\n"
			" - DSP on Dedicated Thread [OFF]\n"
			" - Manually set the extensions for each Wiimote\n"
			"\n"
			"All players should use the same Dolphin version and settings.\n"
			"All memory cards must be identical between players or disabled.\n"
			"Wiimote support is probably terrible. Don't use it.\n"
			"\n"
			"If connecting directly host must have the chosen UDP port open/forwarded!\n"));

		wxBoxSizer* const top_szr = new wxBoxSizer(wxHORIZONTAL);

		top_szr->Add(m_ip_lbl, 0, wxCENTER | wxRIGHT, 5);
		top_szr->Add(m_connect_ip_text, 3);
		top_szr->Add(m_client_port_lbl, 0, wxCENTER | wxRIGHT | wxLEFT, 5);
		top_szr->Add(m_connect_port_text, 1);

		wxBoxSizer* const con_szr = new wxBoxSizer(wxVERTICAL);
		con_szr->Add(top_szr, 0, wxALL | wxEXPAND, 5);
		con_szr->AddStretchSpacer(1);
		con_szr->Add(alert_lbl, 0, wxLEFT | wxRIGHT | wxEXPAND, 5);
		con_szr->AddStretchSpacer(1);
		con_szr->Add(connect_btn, 0, wxALL | wxALIGN_RIGHT, 5);

		connect_tab->SetSizerAndFit(con_szr);
	}

	// host tab
	{
		m_host_port_lbl = new wxStaticText(host_tab, wxID_ANY, _("Port :"));

		// string? w/e
		std::string port;
		netplay_section.Get("HostPort", &port, "2626");
		m_host_port_text = new wxTextCtrl(host_tab, wxID_ANY, StrToWxStr(port));

		wxButton* const host_btn = new wxButton(host_tab, wxID_ANY, _("Host"));
		host_btn->Bind(wxEVT_BUTTON, &NetPlaySetupDiag::OnHost, this);

		m_game_lbox = new wxListBox(host_tab, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
		m_game_lbox->Bind(wxEVT_LISTBOX_DCLICK, &NetPlaySetupDiag::OnHost, this);

		FillWithGameNames(m_game_lbox, *game_list);

		wxBoxSizer* const top_szr = new wxBoxSizer(wxHORIZONTAL);
		top_szr->Add(m_host_port_lbl, 0, wxCENTER | wxRIGHT, 5);
		top_szr->Add(m_host_port_text, 0);
#ifdef USE_UPNP
		m_upnp_chk = new wxCheckBox(host_tab, wxID_ANY, _("Forward port (UPnP)"));
		top_szr->Add(m_upnp_chk, 0, wxALL | wxALIGN_RIGHT, 5);
#endif

		wxBoxSizer* const host_szr = new wxBoxSizer(wxVERTICAL);
		host_szr->Add(top_szr, 0, wxALL | wxEXPAND, 5);
		host_szr->Add(m_game_lbox, 1, wxLEFT | wxRIGHT | wxEXPAND, 5);
		host_szr->Add(host_btn, 0, wxALL | wxALIGN_RIGHT, 5);

		host_tab->SetSizerAndFit(host_szr);
	}

	// bottom row
	wxButton* const quit_btn = new wxButton(panel, wxID_ANY, _("Quit"));
	quit_btn->Bind(wxEVT_BUTTON, &NetPlaySetupDiag::OnQuit, this);

	// main sizer
	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	main_szr->Add(trav_szr, 0, wxALL | wxALIGN_LEFT);
	main_szr->Add(nick_szr, 0, wxALL | wxALIGN_LEFT, 5);
	main_szr->Add(notebook, 1, wxLEFT | wxRIGHT | wxEXPAND, 5);
	main_szr->Add(quit_btn, 0, wxALL | wxALIGN_RIGHT, 5);

	panel->SetSizerAndFit(main_szr);

	//wxBoxSizer* const diag_szr = new wxBoxSizer(wxVERTICAL);
	//diag_szr->Add(panel, 1, wxEXPAND);
	//SetSizerAndFit(diag_szr);

	main_szr->SetSizeHints(this);

	Center();
	Show();

	//  Needs to be done last or it set up the spacing on the page correctly
	wxCommandEvent ev;
	OnChoice(ev);

}

NetPlaySetupDiag::~NetPlaySetupDiag()
{
	IniFile inifile;
	const std::string dolphin_ini = File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini";
	inifile.Load(dolphin_ini);
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	std::string travChoice = "traversal";
	if (m_direct_traversal->GetSelection() == 1)
	{
		netplay_section.Set("TraversalChoice", travChoice);
	}
	else
	{
		travChoice = "direct";
		netplay_section.Set("TraversalChoice", travChoice);
	}

	netplay_section.Set("Nickname", WxStrToStr(m_nickname_text->GetValue()));
	netplay_section.Set("TraversalServer", WxStrToStr(m_traversal_server->GetValue()));
	netplay_section.Set("TraversalPort", WxStrToStr(m_traversal_port->GetValue()));

	if (m_direct_traversal->GetCurrentSelection() == 0)
	{
		netplay_section.Set("Address", WxStrToStr(m_connect_ip_text->GetValue()));
	}
	else
	{
		netplay_section.Set("HostCode", WxStrToStr(m_connect_ip_text->GetValue()));
	}
	netplay_section.Set("ConnectPort", WxStrToStr(m_connect_port_text->GetValue()));
	netplay_section.Set("HostPort", WxStrToStr(m_host_port_text->GetValue()));

	inifile.Save(dolphin_ini);
	main_frame->g_NetPlaySetupDiag = nullptr;
}

void NetPlaySetupDiag::MakeNetPlayDiag(int port, const std::string &game, bool is_hosting)
{
	NetPlayDiag *&npd = NetPlayDiag::GetInstance();
	std::string ip;
	npd = new NetPlayDiag(m_parent, m_game_list, game, is_hosting);
	if (is_hosting)
		ip = "127.0.0.1";
	else
		ip = WxStrToStr(m_connect_ip_text->GetValue());

	bool trav;
	if (!is_hosting && m_direct_traversal->GetCurrentSelection() == 1)
		trav = true;
	else
		trav = false;

	unsigned long centralPort = 0;
	m_traversal_port->GetValue().ToULong(&centralPort);
	netplay_client = new NetPlayClient(ip, (u16)port, npd, WxStrToStr(m_nickname_text->GetValue()), trav, WxStrToStr(m_traversal_server->GetValue()), (u16)centralPort);
	if (netplay_client->is_connected)
	{
		npd->Show();
		Destroy();
	}
	else
	{
		npd->Destroy();
	}
}

void NetPlaySetupDiag::OnHost(wxCommandEvent&)
{
	NetPlayDiag *&npd = NetPlayDiag::GetInstance();
	if (npd)
	{
		WxUtils::ShowErrorDialog(_("A NetPlay window is already open!"));
		return;
	}

	if (-1 == m_game_lbox->GetSelection())
	{
		WxUtils::ShowErrorDialog(_("You must choose a game!"));
		return;
	}

	std::string game(WxStrToStr(m_game_lbox->GetStringSelection()));

	bool trav;
	if (m_direct_traversal->GetCurrentSelection() == 1)
		trav = true;
	else
		trav = false;

	unsigned long port = 0;
	m_host_port_text->GetValue().ToULong(&port);

	unsigned long centralPort = 0;
	m_traversal_port->GetValue().ToULong(&centralPort);
	netplay_server = new NetPlayServer(u16(port), trav, WxStrToStr(m_traversal_server->GetValue()), u16(centralPort));
	if (netplay_server->is_connected)
	{
		netplay_server->ChangeGame(game);
		netplay_server->AdjustPadBufferSize(INITIAL_PAD_BUFFER_SIZE);
#ifdef USE_UPNP
		if (m_upnp_chk->GetValue())
			netplay_server->TryPortmapping(port);
#endif
		MakeNetPlayDiag(netplay_server->GetPort(), game, true);
		netplay_server->SetNetPlayUI(NetPlayDiag::GetInstance());
	}
	else
	{
		WxUtils::ShowErrorDialog(_("Failed to listen. Is another instance of the NetPlay server running?"));
	}
}

void NetPlaySetupDiag::OnJoin(wxCommandEvent&)
{
	NetPlayDiag *&npd = NetPlayDiag::GetInstance();
	if (npd)
	{
		WxUtils::ShowErrorDialog(_("A NetPlay window is already open!"));
		return;
	}

	unsigned long port = 0;
	m_connect_port_text->GetValue().ToULong(&port);
	MakeNetPlayDiag(port, "", false);
}

void NetPlaySetupDiag::OnChoice(wxCommandEvent& event)
{
	int sel = m_direct_traversal->GetSelection();
	IniFile inifile;
	inifile.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	if (sel == 1)
	{
		m_traversal_server_lbl->Show(true);
		m_traversal_server->Show(true);

		m_traversal_port_lbl->Show(true);
		m_traversal_port->Show(true);

		//Traversal
		//client tab
		{
			m_ip_lbl->SetLabelText("Host Code: ");

			std::string address;
			netplay_section.Get("HostCode", &address, "00000000");
			m_connect_ip_text->SetLabelText(address);

			m_client_port_lbl->Show(false);
			m_connect_port_text->Show(false);
		}

		//server tab
		{
			m_host_port_lbl->Show(false);
			m_host_port_text->Show(false);
			m_upnp_chk->Show(false);
		}
	}
	else
	{
		m_traversal_server_lbl->Show(false);
		m_traversal_server->Show(false);

		m_traversal_port_lbl->Show(false);
		m_traversal_port->Show(false);
		//Direct
		//client tab
		{
			m_ip_lbl->SetLabelText("IP Address :");

			std::string address;
			netplay_section.Get("Address", &address, "127.0.0.1");
			m_connect_ip_text->SetLabelText(address);

			m_client_port_lbl->Show(true);
			m_connect_port_text->Show(true);
		}

		//server tab
		{
			m_host_port_lbl->Show(true);
			m_host_port_text->Show(true);
			m_upnp_chk->Show(true);
		}
	}
}

void NetPlaySetupDiag::OnQuit(wxCommandEvent&)
{
	Destroy();
}

NetPlayDiag::NetPlayDiag(wxWindow* const parent, const CGameListCtrl* const game_list,
	const std::string& game, const bool is_hosting)
	: wxFrame(parent, wxID_ANY, _("Dolphin NetPlay"))
	, m_selected_game(game)
	, m_start_btn(nullptr)
	, m_host_label(nullptr)
	, m_host_type_choice(nullptr)
	, m_host_copy_btn(nullptr)
	, m_host_copy_btn_is_retry(false)
	, m_is_hosting(is_hosting)
	, m_game_list(game_list)
{
	Bind(wxEVT_THREAD, &NetPlayDiag::OnThread, this);

	wxPanel* const panel = new wxPanel(this);

	// top crap
	m_game_btn = new wxButton(panel, wxID_ANY,
		StrToWxStr(m_selected_game).Prepend(_(" Game : ")),
		wxDefaultPosition, wxDefaultSize, wxBU_LEFT);

	if (m_is_hosting)
		m_game_btn->Bind(wxEVT_BUTTON, &NetPlayDiag::OnChangeGame, this);
	else
		m_game_btn->Disable();

	// middle crap

	// chat
	m_chat_text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString
		, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_MULTILINE);

	m_chat_msg_text = new wxTextCtrl(panel, wxID_ANY, wxEmptyString
		, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	m_chat_msg_text->Bind(wxEVT_TEXT_ENTER, &NetPlayDiag::OnChat, this);
	m_chat_msg_text->SetMaxLength(2000);

	wxButton* const chat_msg_btn = new wxButton(panel, wxID_ANY, _("Send"));
	chat_msg_btn->Bind(wxEVT_BUTTON, &NetPlayDiag::OnChat, this);

	wxBoxSizer* const chat_msg_szr = new wxBoxSizer(wxHORIZONTAL);
	chat_msg_szr->Add(m_chat_msg_text, 1);
	chat_msg_szr->Add(chat_msg_btn, 0);

	wxStaticBoxSizer* const chat_szr = new wxStaticBoxSizer(wxVERTICAL, panel, _("Chat"));
	chat_szr->Add(m_chat_text, 1, wxEXPAND);
	chat_szr->Add(chat_msg_szr, 0, wxEXPAND | wxTOP, 5);

	m_player_lbox = new wxListBox(panel, wxID_ANY, wxDefaultPosition, wxSize(256, -1));

	wxStaticBoxSizer* const player_szr = new wxStaticBoxSizer(wxVERTICAL, panel, _("Players"));

	// player list
	if (m_is_hosting && g_TraversalClient)
	{
		wxBoxSizer* const host_szr = new wxBoxSizer(wxHORIZONTAL);
		m_host_type_choice = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxSize(60, -1));
		m_host_type_choice->Bind(wxEVT_COMMAND_CHOICE_SELECTED, &NetPlayDiag::OnChoice, this);
		m_host_type_choice->Append(_("ID:"));
		host_szr->Add(m_host_type_choice);

		m_host_label = new wxStaticText(panel, wxID_ANY, "555.555.555.555:55555", wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE | wxALIGN_LEFT);
		// Update() should fix this immediately.
		m_host_label->SetLabel(_(""));
		host_szr->Add(m_host_label, 1, wxLEFT | wxCENTER, 5);

		m_host_copy_btn = new wxButton(panel, wxID_ANY, _("Copy"));
		m_host_copy_btn->Bind(wxEVT_COMMAND_BUTTON_CLICKED, &NetPlayDiag::OnCopyIP, this);
		m_host_copy_btn->Disable();
		host_szr->Add(m_host_copy_btn, 0, wxLEFT | wxCENTER, 5);
		player_szr->Add(host_szr, 0, wxEXPAND | wxBOTTOM, 5);
		m_host_type_choice->Select(0);

		UpdateHostLabel();
	}

	player_szr->Add(m_player_lbox, 1, wxEXPAND);

	if (m_is_hosting)
	{
		m_player_lbox->Bind(wxEVT_LISTBOX, &NetPlayDiag::OnPlayerSelect, this);
		m_kick_btn = new wxButton(panel, wxID_ANY, _("Kick Player"));
		m_kick_btn->Bind(wxEVT_BUTTON, &NetPlayDiag::OnKick, this);
		player_szr->Add(m_kick_btn, 0, wxEXPAND | wxTOP, 5);
		m_kick_btn->Disable();

		m_player_config_btn = new wxButton(panel, wxID_ANY, _("Configure Pads"));
		m_player_config_btn->Bind(wxEVT_BUTTON, &NetPlayDiag::OnConfigPads, this);
		player_szr->Add(m_player_config_btn, 0, wxEXPAND | wxTOP, 5);
	}

	wxBoxSizer* const mid_szr = new wxBoxSizer(wxHORIZONTAL);
	mid_szr->Add(chat_szr, 1, wxEXPAND | wxRIGHT, 5);
	mid_szr->Add(player_szr, 0, wxEXPAND);

	// bottom crap
	wxButton* const quit_btn = new wxButton(panel, wxID_ANY, _("Quit"));
	quit_btn->Bind(wxEVT_BUTTON, &NetPlayDiag::OnQuit, this);

	wxBoxSizer* const bottom_szr = new wxBoxSizer(wxHORIZONTAL);
	if (is_hosting)
	{
		m_start_btn = new wxButton(panel, wxID_ANY, _("Start"));
		m_start_btn->Bind(wxEVT_BUTTON, &NetPlayDiag::OnStart, this);
		bottom_szr->Add(m_start_btn);

		bottom_szr->Add(new wxStaticText(panel, wxID_ANY, _("Buffer:")), 0, wxLEFT | wxCENTER, 5);
		wxSpinCtrl* const padbuf_spin = new wxSpinCtrl(panel, wxID_ANY, std::to_string(INITIAL_PAD_BUFFER_SIZE)
			, wxDefaultPosition, wxSize(64, -1), wxSP_ARROW_KEYS, 0, 200, INITIAL_PAD_BUFFER_SIZE);
		padbuf_spin->Bind(wxEVT_SPINCTRL, &NetPlayDiag::OnAdjustBuffer, this);
		bottom_szr->Add(padbuf_spin, 0, wxCENTER);

		m_memcard_write = new wxCheckBox(panel, wxID_ANY, _("Write memcards (GC)"));
		bottom_szr->Add(m_memcard_write, 0, wxCENTER);
	}

	m_record_chkbox = new wxCheckBox(panel, wxID_ANY, _("Record input"));
	bottom_szr->Add(m_record_chkbox, 0, wxCENTER);

	bottom_szr->AddStretchSpacer(1);
	bottom_szr->Add(quit_btn);

	// main sizer
	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	main_szr->Add(m_game_btn, 0, wxEXPAND | wxALL, 5);
	main_szr->Add(mid_szr, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);
	main_szr->Add(bottom_szr, 0, wxEXPAND | wxALL, 5);

	panel->SetSizerAndFit(main_szr);

	main_szr->SetSizeHints(this);
	SetSize(512, 512 - 128);

	Center();
}

NetPlayDiag::~NetPlayDiag()
{
	if (netplay_client)
	{
		delete netplay_client;
		netplay_client = nullptr;
	}
	if (netplay_server)
	{
		delete netplay_server;
		netplay_server = nullptr;
	}
	npd = nullptr;
}

void NetPlayDiag::OnChat(wxCommandEvent&)
{
	wxString text = m_chat_msg_text->GetValue();

	if (!text.empty())
	{
		netplay_client->SendChatMessage(WxStrToStr(text));
		m_chat_text->AppendText(text.Prepend(" >> ").Append('\n'));
		m_chat_msg_text->Clear();
	}
}

void NetPlayDiag::GetNetSettings(NetSettings &settings)
{
	SConfig &instance = SConfig::GetInstance();
	settings.m_CPUthread = instance.m_LocalCoreStartupParameter.bCPUThread;
	settings.m_CPUcore = instance.m_LocalCoreStartupParameter.iCPUCore;
	settings.m_DSPHLE = instance.m_LocalCoreStartupParameter.bDSPHLE;
	settings.m_DSPEnableJIT = instance.m_DSPEnableJIT;
	settings.m_WriteToMemcard = m_memcard_write->GetValue();
	settings.m_OCEnable = instance.m_OCEnable;
	settings.m_OCFactor = instance.m_OCFactor;
	settings.m_EXIDevice[0] = instance.m_EXIDevice[0];
	settings.m_EXIDevice[1] = instance.m_EXIDevice[1];
}

std::string NetPlayDiag::FindGame()
{
	// find path for selected game, sloppy..
	for (u32 i = 0; auto game = m_game_list->GetISO(i); ++i)
		if (m_selected_game == BuildGameName(*game))
			return game->GetFileName();

	WxUtils::ShowErrorDialog(_("Game not found!"));
	return "";
}

void NetPlayDiag::OnStart(wxCommandEvent&)
{
	NetSettings settings;
	GetNetSettings(settings);
	netplay_server->SetNetSettings(settings);
	netplay_server->StartGame();
}

void NetPlayDiag::BootGame(const std::string& filename)
{
	main_frame->BootGame(filename);
}

void NetPlayDiag::StopGame()
{
	main_frame->DoStop();
}

// NetPlayUI methods called from ---NETPLAY--- thread
void NetPlayDiag::Update()
{
	wxThreadEvent evt(wxEVT_THREAD, 1);
	GetEventHandler()->AddPendingEvent(evt);
}

void NetPlayDiag::AppendChat(const std::string& msg)
{
	chat_msgs.Push(msg);
	// silly
	Update();
}

void NetPlayDiag::OnMsgChangeGame(const std::string& filename)
{
	wxThreadEvent* evt = new wxThreadEvent(wxEVT_THREAD, NP_GUI_EVT_CHANGE_GAME);
	evt->SetString(StrToWxStr(filename));
	GetEventHandler()->QueueEvent(evt);
}

void NetPlayDiag::OnMsgStartGame()
{
	wxThreadEvent evt(wxEVT_THREAD, NP_GUI_EVT_START_GAME);
	GetEventHandler()->AddPendingEvent(evt);
	if (m_is_hosting)
	{
		m_start_btn->Disable();
		m_memcard_write->Disable();
		m_game_btn->Disable();
		m_player_config_btn->Disable();
	}

	m_record_chkbox->Disable();
}

void NetPlayDiag::OnMsgStopGame()
{
	wxThreadEvent evt(wxEVT_THREAD, NP_GUI_EVT_STOP_GAME);
	GetEventHandler()->AddPendingEvent(evt);
	if (m_is_hosting)
	{
		m_start_btn->Enable();
		m_memcard_write->Enable();
		m_game_btn->Enable();
		m_player_config_btn->Enable();
	}
	m_record_chkbox->Enable();
}

void NetPlayDiag::OnAdjustBuffer(wxCommandEvent& event)
{
	const int val = ((wxSpinCtrl*)event.GetEventObject())->GetValue();
	netplay_server->AdjustPadBufferSize(val);

	std::ostringstream ss;
	ss << "< Pad Buffer: " << val << " >";
	netplay_client->SendChatMessage(ss.str());
	m_chat_text->AppendText(StrToWxStr(ss.str()).Append('\n'));
}

void NetPlayDiag::OnQuit(wxCommandEvent&)
{
	Destroy();
}

// update gui
void NetPlayDiag::OnThread(wxThreadEvent& event)
{
	if (m_is_hosting && m_host_label && g_TraversalClient)
	{
		UpdateHostLabel();
	}

	// player list
	m_playerids.clear();
	std::string tmps;
	netplay_client->GetPlayerList(tmps, m_playerids);

	wxString selection;
	if (m_player_lbox->GetSelection() != wxNOT_FOUND)
		selection = m_player_lbox->GetString(m_player_lbox->GetSelection());

	m_player_lbox->Clear();
	std::istringstream ss(tmps);
	while (std::getline(ss, tmps))
		m_player_lbox->Append(StrToWxStr(tmps));

	// remove ping from selection string, in case it has changed
	selection.erase(selection.find_last_of("|") + 1);

	if (!selection.empty())
	{
		for (unsigned int i = 0; i < m_player_lbox->GetCount(); ++i)
		{
			if (selection == m_player_lbox->GetString(i).Mid(0, selection.length()))
			{
				m_player_lbox->SetSelection(i);
				break;
			}
		}
	}

	// flash window in taskbar when someone joins if window isn't active
	static u8 numPlayers = 1;
	bool focus = (wxWindow::FindFocus() == this || (wxWindow::FindFocus() != nullptr && wxWindow::FindFocus()->GetParent() == this) ||
		(wxWindow::FindFocus() != nullptr && wxWindow::FindFocus()->GetParent() != nullptr
		&& wxWindow::FindFocus()->GetParent()->GetParent() == this));
	if (netplay_server != nullptr && numPlayers < m_playerids.size() && !focus)
	{
		RequestUserAttention();
	}
	numPlayers = m_playerids.size();

	switch (event.GetId())
	{
	case NP_GUI_EVT_CHANGE_GAME:
		// update selected game :/
	{
		m_selected_game.assign(WxStrToStr(event.GetString()));

		wxString button_label = event.GetString();
		m_game_btn->SetLabel(button_label.Prepend(_(" Game : ")));
	}
	break;
	case NP_GUI_EVT_START_GAME:
		// client start game :/
	{
		netplay_client->StartGame(FindGame());
	}
	break;
	case NP_GUI_EVT_STOP_GAME:
		// client stop game
	{
		netplay_client->StopGame();
	}
	break;
	}

	// chat messages
	while (chat_msgs.Size())
	{
		std::string s;
		chat_msgs.Pop(s);
		//PanicAlert("message: %s", s.c_str());
		m_chat_text->AppendText(StrToWxStr(s).Append('\n'));
	}
}

void NetPlayDiag::OnChangeGame(wxCommandEvent&)
{
	wxString game_name;
	ChangeGameDiag* const cgd = new ChangeGameDiag(this, m_game_list, game_name);
	cgd->ShowModal();

	if (game_name.length())
	{
		m_selected_game = WxStrToStr(game_name);
		netplay_server->ChangeGame(m_selected_game);
		m_game_btn->SetLabel(game_name.Prepend(_(" Game : ")));
	}
}

void NetPlayDiag::OnConfigPads(wxCommandEvent&)
{
	PadMapping mapping[4];
	PadMapping wiimotemapping[4];
	std::vector<const Player *> player_list;
	netplay_server->GetPadMapping(mapping);
	netplay_server->GetWiimoteMapping(wiimotemapping);
	netplay_client->GetPlayers(player_list);
	PadMapDiag pmd(this, mapping, wiimotemapping, player_list);
	pmd.ShowModal();
	netplay_server->SetPadMapping(mapping);
	netplay_server->SetWiimoteMapping(wiimotemapping);
}

void NetPlayDiag::OnKick(wxCommandEvent&)
{
	wxString selection = m_player_lbox->GetStringSelection();
	unsigned long player = 0;
	selection.Mid(selection.find_last_of("[") + 1, selection.find_last_of("]")).ToULong(&player);

	netplay_server->KickPlayer((u8)player);

	m_player_lbox->SetSelection(wxNOT_FOUND);
	wxCommandEvent event;
	OnPlayerSelect(event);
}

void NetPlayDiag::OnPlayerSelect(wxCommandEvent&)
{
	if (m_player_lbox->GetSelection() > 0)
		m_kick_btn->Enable();
	else
		m_kick_btn->Disable();
}

bool NetPlayDiag::IsRecording()
{
	return m_record_chkbox->GetValue();
}


void NetPlayDiag::OnCopyIP(wxCommandEvent&)
{
	if (m_host_copy_btn_is_retry)
	{
		g_TraversalClient->ReconnectToServer();
		Update();
	}
	else
	{
		if (wxTheClipboard->Open())
		{
			wxTheClipboard->SetData(new wxTextDataObject(m_host_label->GetLabel()));
			wxTheClipboard->Close();
		}
	}
}

void NetPlayDiag::OnChoice(wxCommandEvent& event)
{
	UpdateHostLabel();
}


void NetPlayDiag::UpdateHostLabel()
{
	wxString label = _(" (internal IP)");
	auto DeLabel = [=](wxString str) {
		if (str == _("Localhost"))
			return std::string("!local!");
		return WxStrToStr(str.Left(str.Len() - label.Len()));
	};
	auto EnLabel = [=](std::string str) -> wxString {
		if (str == "!local!")
			return _("Localhost");
		return StrToWxStr(str) + label;
	};
	int sel = m_host_type_choice->GetSelection();
	if (sel == 0)
	{
		// the traversal ID
		switch (g_TraversalClient->m_State)
		{
		case TraversalClient::Connecting:
			m_host_label->SetForegroundColour(*wxLIGHT_GREY);
			m_host_label->SetLabel("...");
			m_host_copy_btn->SetLabel(_("Copy"));
			m_host_copy_btn->Disable();
			break;
		case TraversalClient::Connected:
			m_host_label->SetForegroundColour(*wxBLACK);
			m_host_label->SetLabel(wxString(g_TraversalClient->m_HostId.data(), g_TraversalClient->m_HostId.size()));
			m_host_copy_btn->SetLabel(_("Copy"));
			m_host_copy_btn->Enable();
			m_host_copy_btn_is_retry = false;
			break;
		case TraversalClient::Failure:
			m_host_label->SetForegroundColour(*wxBLACK);
			m_host_label->SetLabel(FailureReasonStringForHostLabel(g_TraversalClient->m_FailureReason));
			m_host_copy_btn->SetLabel(_("Retry"));
			m_host_copy_btn->Enable();
			m_host_copy_btn_is_retry = true;
			break;
		}
	}
	else if (sel != wxNOT_FOUND) // wxNOT_FOUND shouldn't generally happen
	{
		m_host_label->SetForegroundColour(*wxBLACK);
		m_host_label->SetLabel(netplay_server->GetInterfaceHost(DeLabel(m_host_type_choice->GetString(sel))));
		m_host_copy_btn->SetLabel(_("Copy"));
		m_host_copy_btn->Enable();
		m_host_copy_btn_is_retry = false;
	}

	auto set = netplay_server->GetInterfaceSet();
	for (const std::string& iface : set)
	{
		wxString wxIface = EnLabel(iface);
		if (m_host_type_choice->FindString(wxIface) == wxNOT_FOUND)
			m_host_type_choice->Append(wxIface);
	}
	for (unsigned i = 1, count = m_host_type_choice->GetCount(); i != count; i++)
	{
		if (set.find(DeLabel(m_host_type_choice->GetString(i))) == set.end())
		{
			m_host_type_choice->Delete(i);
			i--;
			count--;
		}
	}
}

ChangeGameDiag::ChangeGameDiag(wxWindow* const parent, const CGameListCtrl* const game_list, wxString& game_name)
	: wxDialog(parent, wxID_ANY, _("Change Game"))
	, m_game_name(game_name)
{
	m_game_lbox = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
	m_game_lbox->Bind(wxEVT_LISTBOX_DCLICK, &ChangeGameDiag::OnPick, this);

	FillWithGameNames(m_game_lbox, *game_list);

	wxButton* const ok_btn = new wxButton(this, wxID_OK, _("Change"));
	ok_btn->Bind(wxEVT_BUTTON, &ChangeGameDiag::OnPick, this);

	wxBoxSizer* const szr = new wxBoxSizer(wxVERTICAL);
	szr->Add(m_game_lbox, 1, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 5);
	szr->Add(ok_btn, 0, wxALL | wxALIGN_RIGHT, 5);

	SetSizerAndFit(szr);
	SetFocus();
}

void ChangeGameDiag::OnPick(wxCommandEvent& event)
{
	// return the selected game name
	m_game_name = m_game_lbox->GetStringSelection();
	EndModal(wxID_OK);
}

PadMapDiag::PadMapDiag(wxWindow* const parent, PadMapping map[], PadMapping wiimotemap[], std::vector<const Player *>& player_list)
	: wxDialog(parent, wxID_ANY, _("Configure Pads"))
	, m_mapping(map)
	, m_wiimapping(wiimotemap)
	, m_player_list(player_list)
{
	wxBoxSizer* const h_szr = new wxBoxSizer(wxHORIZONTAL);
	h_szr->AddSpacer(10);

	wxArrayString player_names;
	player_names.Add(_("None"));
	for (auto& player : m_player_list)
		player_names.Add(player->name);

	for (unsigned int i = 0; i < 4; ++i)
	{
		wxBoxSizer* const v_szr = new wxBoxSizer(wxVERTICAL);
		v_szr->Add(new wxStaticText(this, wxID_ANY, (wxString(_("Pad ")) + (wxChar)('0' + i))),
		                            1, wxALIGN_CENTER_HORIZONTAL);

		m_map_cbox[i] = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, player_names);
		m_map_cbox[i]->Bind(wxEVT_CHOICE, &PadMapDiag::OnAdjust, this);
		if (m_mapping[i] == -1)
			m_map_cbox[i]->Select(0);
		else
			for (unsigned int j = 0; j < m_player_list.size(); j++)
				if (m_mapping[i] == m_player_list[j]->pid)
					m_map_cbox[i]->Select(j + 1);

		v_szr->Add(m_map_cbox[i], 1);

		h_szr->Add(v_szr, 1, wxTOP | wxEXPAND, 20);
		h_szr->AddSpacer(10);
	}

	for (unsigned int i = 0; i < 4; ++i)
	{
		wxBoxSizer* const v_szr = new wxBoxSizer(wxVERTICAL);
		v_szr->Add(new wxStaticText(this, wxID_ANY, (wxString(_("Wiimote ")) + (wxChar)('0' + i))),
		                            1, wxALIGN_CENTER_HORIZONTAL);

		m_map_cbox[i + 4] = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, player_names);
		m_map_cbox[i + 4]->Bind(wxEVT_CHOICE, &PadMapDiag::OnAdjust, this);
		if (m_wiimapping[i] == -1)
			m_map_cbox[i + 4]->Select(0);
		else
			for (unsigned int j = 0; j < m_player_list.size(); j++)
				if (m_wiimapping[i] == m_player_list[j]->pid)
					m_map_cbox[i + 4]->Select(j + 1);

		v_szr->Add(m_map_cbox[i + 4], 1);

		h_szr->Add(v_szr, 1, wxTOP | wxEXPAND, 20);
		h_szr->AddSpacer(10);
	}

	wxBoxSizer* const main_szr = new wxBoxSizer(wxVERTICAL);
	main_szr->Add(h_szr);
	main_szr->AddSpacer(5);
	main_szr->Add(CreateButtonSizer(wxOK), 0, wxEXPAND | wxLEFT | wxRIGHT, 20);
	main_szr->AddSpacer(5);
	SetSizerAndFit(main_szr);
	SetFocus();
}

void PadMapDiag::OnAdjust(wxCommandEvent& event)
{
	(void)event;
	for (unsigned int i = 0; i < 4; i++)
	{
		int player_idx = m_map_cbox[i]->GetSelection();
		if (player_idx > 0)
			m_mapping[i] = m_player_list[player_idx - 1]->pid;
		else
			m_mapping[i] = -1;

		player_idx = m_map_cbox[i + 4]->GetSelection();
		if (player_idx > 0)
			m_wiimapping[i] = m_player_list[player_idx - 1]->pid;
		else
			m_wiimapping[i] = -1;
	}
}

void NetPlay::StopGame()
{
	if (netplay_client != nullptr)
		netplay_client->Stop();
}
