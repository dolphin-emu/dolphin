//IMPORTANT: These patches are to applied through the C++ preprocessor
//This is to ease the development of future versions and prevent merge conflicts with FasterMelee and Dolphin

//Adds 'netplay' argument to dolphin
#define ADD_ARGUMENT { wxCMD_LINE_OPTION, "n", "netplay", "Starts/Joins a netplay server", wxCMD_LINE_VAL_STRING,\
	wxCMD_LINE_PARAM_OPTIONAL },

#define ADD_PARSER m_netplay = parser.Found("netplay", &m_netplay_code);

#define GLOBAL_INITALIZATION \
wxString m_netplay_code;\
bool m_netplay;
