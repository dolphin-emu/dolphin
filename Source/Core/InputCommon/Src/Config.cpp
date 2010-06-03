
#include "Config.h"

Plugin::Plugin( const char* const _ini_name, const char* const _gui_name, const char* const _profile_name  )
	: ini_name(_ini_name)
	, gui_name(_gui_name)
	, profile_name(_profile_name)
{
	// GCPads
	//for ( unsigned int i = 0; i<4; ++i )
		//controllers.push_back( new GCPad( i ) );
	// Wiimotes / disabled, cause it only the GUI half is done
	//for ( unsigned int i = 0; i<4; ++i )
	//	controllers.push_back( new Wiimote( i ) );
};

Plugin::~Plugin()
{
	// delete pads
	std::vector<ControllerEmu*>::const_iterator i = controllers.begin(),
		e = controllers.end();
	for ( ; i != e; ++i )
		delete *i;
}

void Plugin::LoadConfig()
{
	IniFile inifile;

	std::ifstream file;
	file.open( (std::string(File::GetUserPath(D_CONFIG_IDX)) + ini_name + ".ini" ).c_str() );
	inifile.Load( file );
	file.close();

	std::vector< ControllerEmu* >::const_iterator i = controllers.begin(),
		e = controllers.end();
	for ( ; i!=e; ++i )
		(*i)->LoadConfig( inifile[ (*i)->GetName() ] );
}

void Plugin::SaveConfig()
{
	IniFile inifile;

	std::vector< ControllerEmu* >::const_iterator i = controllers.begin(),
		e = controllers.end();
	for ( ; i!=e; ++i )
		(*i)->SaveConfig( inifile[ (*i)->GetName() ] );
	
	// dont need to save empty values
	//inifile.Clean();

	std::ofstream file;
	file.open( (std::string(File::GetUserPath(D_CONFIG_IDX)) + ini_name + ".ini" ).c_str() );
	inifile.Save( file );
	file.close();
}
