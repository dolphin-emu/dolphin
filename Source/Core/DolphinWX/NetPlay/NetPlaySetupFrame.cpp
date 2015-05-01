// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/listbox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Core/NetPlayClient.h"
#include "Core/NetPlayServer.h"
#include "DolphinWX/Frame.h"
#include "DolphinWX/Main.h"
#include "DolphinWX/WxUtils.h"
#include "DolphinWX/NetPlay/NetPlaySetupFrame.h"
#include "DolphinWX/NetPlay/NetWindow.h"

static void GetTraversalPort(IniFile::Section& section, std::string* port)
{
	section.Get("TraversalPort", port, "6262");
	port->erase(std::remove(port->begin(), port->end(), ' '), port->end());
	if (port->empty())
		*port = "6262";
}

static void GetTraversalServer(IniFile::Section& section, std::string* server)
{
	section.Get("TraversalServer", server, "stun.dolphin-emu.org");
	server->erase(std::remove(server->begin(), server->end(), ' '), server->end());
	if (server->empty())
		*server = "stun.dolphin-emu.org";
}

NetPlaySetupFrame::NetPlaySetupFrame(wxWindow* const parent, const CGameListCtrl* const game_list)
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
	m_direct_traversal->Bind(wxEVT_CHOICE, &NetPlaySetupFrame::OnChoice, this);
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
		connect_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnJoin, this);

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
		host_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnHost, this);

		m_game_lbox = new wxListBox(host_tab, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxLB_SORT);
		m_game_lbox->Bind(wxEVT_LISTBOX_DCLICK, &NetPlaySetupFrame::OnHost, this);

		NetPlayDialog::FillWithGameNames(m_game_lbox, *game_list);

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
	quit_btn->Bind(wxEVT_BUTTON, &NetPlaySetupFrame::OnQuit, this);

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

NetPlaySetupFrame::~NetPlaySetupFrame()
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

void NetPlaySetupFrame::MakeNetPlayDiag(int port, const std::string &game, bool is_hosting)
{
	NetPlayDialog*& npd = NetPlayDialog::GetInstance();
	NetPlayClient*& netplay_client = NetPlayDialog::GetNetPlayClient();

	std::string ip;
	npd = new NetPlayDialog(m_parent, m_game_list, game, is_hosting);
	if (is_hosting)
		ip = "127.0.0.1";
	else
		ip = WxStrToStr(m_connect_ip_text->GetValue());

	bool trav;
	if (!is_hosting && m_direct_traversal->GetCurrentSelection() == 1)
		trav = true;
	else
		trav = false;

	IniFile inifile;
	inifile.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	std::string centralPortString;
	GetTraversalPort(netplay_section, &centralPortString);
	unsigned long int centralPort;
	StrToWxStr(centralPortString).ToULong(&centralPort);

	std::string centralServer;
	GetTraversalServer(netplay_section, &centralServer);

	netplay_client = new NetPlayClient(ip, (u16)port, npd, WxStrToStr(m_nickname_text->GetValue()), trav, centralServer, (u16) centralPort);
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

void NetPlaySetupFrame::OnHost(wxCommandEvent&)
{
	NetPlayDialog*& npd = NetPlayDialog::GetInstance();
	NetPlayServer*& netplay_server = NetPlayDialog::GetNetPlayServer();

	if (npd)
	{
		WxUtils::ShowErrorDialog(_("A NetPlay window is already open!"));
		return;
	}

	if (m_game_lbox->GetSelection() == wxNOT_FOUND)
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

	IniFile inifile;
	inifile.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	std::string centralPortString;
	GetTraversalPort(netplay_section, &centralPortString);
	unsigned long int centralPort;
	StrToWxStr(centralPortString).ToULong(&centralPort);

	std::string centralServer;
	GetTraversalServer(netplay_section, &centralServer);

	netplay_server = new NetPlayServer((u16)port, trav, centralServer, (u16) centralPort);
	if (netplay_server->is_connected)
	{
		netplay_server->ChangeGame(game);
		netplay_server->AdjustPadBufferSize(INITIAL_PAD_BUFFER_SIZE);
#ifdef USE_UPNP
		if (m_upnp_chk->GetValue())
			netplay_server->TryPortmapping(port);
#endif
		MakeNetPlayDiag(netplay_server->GetPort(), game, true);
		netplay_server->SetNetPlayUI(NetPlayDialog::GetInstance());
	}
	else
	{
		WxUtils::ShowErrorDialog(_("Failed to listen. Is another instance of the NetPlay server running?"));
	}
}

void NetPlaySetupFrame::OnJoin(wxCommandEvent&)
{
	NetPlayDialog*& npd = NetPlayDialog::GetInstance();
	if (npd)
	{
		WxUtils::ShowErrorDialog(_("A NetPlay window is already open!"));
		return;
	}

	unsigned long port = 0;
	m_connect_port_text->GetValue().ToULong(&port);
	MakeNetPlayDiag(port, "", false);
}

void NetPlaySetupFrame::OnChoice(wxCommandEvent& event)
{
	int sel = m_direct_traversal->GetSelection();
	IniFile inifile;
	inifile.Load(File::GetUserPath(D_CONFIG_IDX) + "Dolphin.ini");
	IniFile::Section& netplay_section = *inifile.GetOrCreateSection("NetPlay");

	if (sel == 1)
	{
		//Traversal
		//client tab
		{
			m_ip_lbl->SetLabelText("Host Code: ");

			std::string address;
			netplay_section.Get("HostCode", &address, "00000000");
			m_connect_ip_text->SetLabelText(address);

			m_client_port_lbl->Hide();
			m_connect_port_text->Hide();
		}

		//server tab
		{
			m_host_port_lbl->Hide();
			m_host_port_text->Hide();
			m_upnp_chk->Hide();
		}
	}
	else
	{
		// Direct
		// Client tab
		{
			m_ip_lbl->SetLabelText("IP Address :");

			std::string address;
			netplay_section.Get("Address", &address, "127.0.0.1");
			m_connect_ip_text->SetLabelText(address);

			m_client_port_lbl->Show();
			m_connect_port_text->Show();
		}

		// Server tab
		m_host_port_lbl->Show();
		m_host_port_text->Show();
		m_upnp_chk->Show();
	}
}

void NetPlaySetupFrame::OnQuit(wxCommandEvent&)
{
	Destroy();
}
