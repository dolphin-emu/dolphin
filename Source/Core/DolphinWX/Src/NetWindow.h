// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _NETWINDOW_H_
#define _NETWINDOW_H_

#include <string>
#include <wx/wx.h>
#include <wx/sizer.h>
#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/gbsizer.h>
#include <wx/listbox.h>

#include <wx/thread.h>

#include "Globals.h"
#include "BootManager.h"
#include "Common.h"
#include "NetStructs.h"
#include "Core.h"
#include "pluginspecs_pad.h"
#include "HW/SI.h"
#include "HW/SI_Device.h"
#include "HW/SI_DeviceGCController.h"
#include "Timer.h"

#ifdef _DEBUG
	#define NET_DEBUG
#endif

// Use TCP instead of UDP to send pad data @ 60fps. Suitable and better for LAN netplay, 
// Unrealistic for Internet netplay, unless you have an uberfast connexion (<10ms ping)
// #define USE_TCP

class ServerSide;
class ClientSide;

class NetPlay : public wxFrame
{
	public:
		NetPlay(wxWindow* parent, std::string GamePath = "", std::string GameName = "");
		~NetPlay();

		void UpdateNetWindow(bool update_infos, wxString=wxT("NULL"));
		void AppendText(const wxString text) { m_Logging->AppendText(text); }

		// Send and receive pads values
		bool GetNetPads(u8 pad_nb, SPADStatus, u32 *netvalues);
		void ChangeSelectedGame(std::string game);
		void IsGameFound(unsigned char*, std::string);
		std::string GetSelectedGame() { wxCriticalSectionLocker lock(m_critical); return m_selectedGame; }

		void LoadGame();

	protected:
		// Protects our vars from being fuxored by threads
		wxCriticalSection  m_critical;

		// this draws the GUI, ya rly
		void DrawGUI();
		void DrawNetWindow();

		// event handlers
		void OnGUIEvent(wxCommandEvent& event);
		void OnDisconnect(wxCommandEvent& event);
		void OnNetEvent(wxCommandEvent& event);
		void OnQuit(wxCloseEvent& event);

		void OnJoin(wxCommandEvent& event);
		void OnHost(wxCommandEvent& event);

		// Net play vars (used ingame)
		int              m_frame;
		int              m_lastframe;
		Common::Timer    m_timer;
		int              m_loopframe;
		int              m_frameDelay;
		bool             m_data_received;// True if first frame data received

		// Basic vars
		std::string      m_paths;		// Game paths list
		std::string      m_games;		// Game names list

		std::string      m_selectedGame;// Selected game's string
		std::string      m_hostaddr;	// Used with OnGetIP to cache it
		bool             m_ready, m_clients_ready;
		std::string      m_nick;

		int              m_NetModel;	// Using P2P model (0) or Server model (1) 
		int              m_isHosting;	// 0 = false ; 1 = true ; 2 = Not set
		unsigned char    m_numClients;	// starting from 0, 4 players max thus 3 clients
		std::string      m_address;     // The address entered into connection box
		unsigned short   m_port;
		
		Netpads          m_pads[4];     // this struct is used to save synced pad values
		IniFile          ConfigIni;

		// Sockets objects
		ServerSide      *m_sock_server;
		ClientSide      *m_sock_client;

		// -----------
		// GUI objects
		// -----------
		wxNotebook      *m_Notebook;
		wxPanel         *m_Tab_Connect;
		wxPanel         *m_Tab_Host;
		wxStaticText    *m_SetNick_text;
		wxTextCtrl      *m_SetNick;
		wxChoice        *m_NetMode;

		// Host tab :
		wxArrayString    m_GameList_str;
		wxStaticText    *m_GameList_text;
		wxListBox       *m_GameList;
		wxStaticText    *m_SetPort_text;
		wxTextCtrl      *m_SetPort;
		wxButton        *m_HostGame;
		wxButton		*m_ExitWindowH;

		// Connect tab :
		wxTextCtrl      *m_ConAddr;
		wxStaticText    *m_ConAddr_text;
		wxButton        *m_JoinGame;
		wxButton		*m_ExitWindowC;
		wxCheckBox      *m_UseRandomPort;

		// Connection window
		wxButton        *m_Game_str;
		wxTextCtrl      *m_Logging;
		wxTextCtrl      *m_Chat;
		wxButton        *m_Chat_ok;
		// Right part
		wxButton        *m_wtfismyip;
		wxButton        *m_ChangeGame;
		// Left Part
		wxButton        *m_Disconnect;
		wxStaticText    *m_ConInfo_text;
		wxButton        *m_GetPing;
		wxCheckBox      *m_Ready;
		wxCheckBox      *m_RecordGame;

		// wxWidgets event table
		DECLARE_EVENT_TABLE()
};

class GameListPopup : public wxDialog
{
	public:
		GameListPopup(NetPlay *net_ptr, wxArrayString GameNames);
		~GameListPopup() {}
	protected:
		void OnButtons(wxCommandEvent& event);
		wxArrayString	 m_GameList_str;
		NetPlay*		 m_netParent;
		wxListBox		*m_GameList;
		wxButton		*m_Accept;
		wxButton		*m_Cancel;
		DECLARE_EVENT_TABLE()
};

enum
{
	ID_NOTEBOOK,
	ID_TAB_HOST,
	ID_TAB_CONN,
	ID_BUTTON_HOST,
	ID_BUTTON_JOIN,
	ID_BUTTON_EXIT,
	ID_NETMODE,
	ID_GAMELIST,
	ID_LOGGING_TXT,
	ID_CHAT,
	ID_SETNICK,
	ID_SETPORT,
	ID_CONNADDR,
	ID_CONNINFO_TXT,
	ID_USE_RANDOMPORT,
	ID_BUTTON_GETPING,
	ID_BUTTON_GETIP,
	ID_CHANGEGAME,
	ID_BUTTON_QUIT,
	ID_BUTTON_CHAT,
	ID_READY,
	ID_RECORD,

	ID_SOCKET,
	ID_SERVER,

	HOST_FULL = 200,	// ...
	HOST_ERROR,			// Sent on socket error
	HOST_DISCONNECTED,
	HOST_NEWPLAYER,
	HOST_PLAYERLEFT,
	CLIENTS_READY,
	CLIENTS_NOTREADY,
	GUI_UPDATE,			// Refresh the shown selectedgame on GUI
	ADD_TEXT,			// Add text to m_Logging (string)
	ADD_INFO,			// Sent when updating net infos (string)
	NET_EVENT
};

#endif // _NETWINDOW_H_

