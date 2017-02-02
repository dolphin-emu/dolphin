//IMPORTANT: These patches are to applied through the C++ preprocessor
//This is to ease the development of future versions and prevent merge conflicts with FasterMelee and Dolphin

//#Usage : Main.cpp
//Adds 'netplay' argument to dolphin
#define ADD_ARGUMENT { wxCMD_LINE_OPTION, "n", "netplay", "Starts/Joins a netplay server", wxCMD_LINE_VAL_STRING,\
	wxCMD_LINE_PARAM_OPTIONAL },

//#Usage : Main.cpp
//Auto connect to netplay
#define ADD_PARSER MeleeNET::m_netplay = parser.Found("netplay", &MeleeNET::m_netplay_code);


//#Usage FrameTools.cpp
//Makes OnNetplaayAccessable publicly
#define ONNETPLAY_PUBLIC void OnNetPlay(wxCommandEvent& event);

//#Usage Main.cpp
//Called after main window initalization
#define AFTER_INIT if(MeleeNET::m_netplay) {\
wxGetApp().GetCFrame()->OnNetPlay(wxCommandEvent());}

#define IF_NETPLAY if (MeleeNET::m_netplay) {\
join_config.use_traversal = true;\
join_config.player_name = "PlaceholderName";\
join_config.game_list_ctrl = m_game_list;\
join_config.SetDialogInfo(netplay_section, m_parent);\
}

#define IF_NETPLAY_SET_CODE join_config.traversal_host = std::string(MeleeNET::m_netplay_code.mb_str());

#define INITALIZE_MELEENET \
wxString MeleeNET::m_netplay_code = wxString("");\
bool MeleeNET::m_netplay = false;\
bool MeleeNET::m_netplay_host = false;



/*
//#Usage NetPlayClient.h
//Def of NETPLAY_CUSTOM in the header file
#define DEFINE_NETPLAY_CUSTOM NetPlayClient(const std::string & address, const u16 port, const std::string & name, bool traversal, const std::string & centralServer, u16 centralPort);

//#Usage NetPlayClient.cpp
//Modified function to run netplay without the need of a UI
#define NETPLAY_CUSTOM NetPlayClient::NetPlayClient(const std::string& address, const u16 port, const std::string& name, bool traversal, const std::string& centralServer, u16 centralPort) : m_player_name(name) { ClearBuffers(); if(traversal) { if (address.size() > NETPLAY_CODE_SIZE) { PanicAlertT("Host code size is to large.\nPlease recheck that you have the correct code"); return; } if (!EnsureTraversalClient(centralServer, centralPort)) return; m_client = g_MainNetHost.get(); m_traversal_client = g_TraversalClient.get(); if (m_traversal_client->m_State == TraversalClient::Failure) m_traversal_client->ReconnectToServer(); m_traversal_client->m_Client = this; m_host_spec = address; m_connection_state = ConnectionState::WaitingForTraversalClientConnection; OnTraversalStateChanged(); m_connecting = true; Common::Timer connect_timer; connect_timer.Start(); while (m_connecting) { ENetEvent netEvent; if (m_traversal_client) m_traversal_client->HandleResends(); while (enet_host_service(m_client, &netEvent, 4) > 0) { sf::Packet rpac; switch (netEvent.type) { case ENET_EVENT_TYPE_CONNECT: m_server = netEvent.peer; if (Connect()) { m_connection_state = ConnectionState::Connected; m_thread = std::thread(&NetPlayClient::ThreadFunc, this); } return; default: break; } } if (connect_timer.GetTimeElapsed() > 5000) break; } PanicAlertT("Failed To Connect!"); } }

//#Usage NetplayClient.h
//Indicates a code block to skip if no UI has been initalized
#define NETPLAY_NOUI if(!m_netplay){
#define NETPLAY_NOUI_END }
*/