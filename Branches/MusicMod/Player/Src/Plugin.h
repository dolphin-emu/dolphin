////////////////////////////////////////////////////////////////////////////////
// Plainamp, Open source Winamp core
// 
// Copyright © 2005  Sebastian Pipping <webmaster@hartwork.org>
// 
// -->  http://www.hartwork.org
// 
// This source code is released under the GNU General Public License (GPL).
// See GPL.txt for details. Any non-GPL usage is strictly forbidden.
////////////////////////////////////////////////////////////////////////////////


#ifndef PA_PLUGIN_H
#define PA_PLUGIN_H



#include "Global.h"
#include <vector>

using namespace std;


class Plugin;
extern vector<Plugin *> plugins;



enum PluginType
{
	PLUGIN_TYPE_INPUT,
	PLUGIN_TYPE_OUTPUT,
	PLUGIN_TYPE_VIS,
	PLUGIN_TYPE_DSP,
	PLUGIN_TYPE_GEN
};



////////////////////////////////////////////////////////////////////////////////
/// Winamp plugin wrapper
////////////////////////////////////////////////////////////////////////////////
class Plugin
{
public:
	Plugin( TCHAR * szDllpath );
	~Plugin();
	
	virtual bool Load()       = 0;
	virtual bool Unload()     = 0;
	
	virtual TCHAR * GetTypeString()  = 0;
	virtual int GetTypeStringLen()   = 0;
	virtual PluginType GetType()     = 0;
	
//	void AllowUnload( bool bAllow ) { bAllowUnload = bAllow; }
	inline  bool IsLoaded() { return ( hDLL != NULL ); }
	virtual bool IsActive() = 0;
	
	inline TCHAR * GetFullpath() { return szFullpath; }
	// inline int GetFullpathLen() { return iFilenameLen; }	
	
	inline TCHAR * GetFilename() { return szFilename; }
	inline int GetFilenameLen() { return iFilenameLen; }	
	inline TCHAR * GetName() { return szName; }
	inline int GetNameLen() { return iNameLen; }
	
	template< class PluginKind >
		static bool FindAll( TCHAR * szPath, TCHAR * szPattern, bool bKeepLoaded );

protected:	
	HINSTANCE    hDLL;               ///< Library handle
	TCHAR *      szName;             ///< Name
	int          iNameLen;           ///< Length of name (in characters)
	
	BOOL iHookerIndex;               ///< Window hook index (0..HC-1). Only last can be unloaded
	WNDPROC WndprocBackup;           ///< Window procedure backup. Is restored when unloading. Only valid for <iHookerIndex != -1>
	static int iWndprocHookCounter;  ///< Number of window hooks (=HC)

private:
	TCHAR *      szFullpath;         ///< Full path e.g. "C:\test.dll"
	TCHAR *      szFilename;         ///< Filename e.g. "test.dll"
	int          iFullpathLen;       ///< Length of full path (in characters)
	int          iFilenameLen;       ///< Length of filename (in characters)
};



////////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////////
template< class PluginKind >
bool Plugin::FindAll( TCHAR * szPath, TCHAR * szPattern, bool bKeepLoaded )
{
	const int uPathLen     = ( int )_tcslen( szPath );
	const int uPatternLen  = ( int )_tcslen( szPattern );

	TCHAR * szFullPattern = new TCHAR[ uPathLen + 1 + uPatternLen + 1 ];
	memcpy( szFullPattern, szPath, uPathLen * sizeof( TCHAR ) );
	szFullPattern[ uPathLen ] = TEXT( '\\' );
	memcpy( szFullPattern + uPathLen + 1, szPattern, uPatternLen * sizeof( TCHAR ) );
	szFullPattern[ uPathLen + 1 + uPatternLen ] = TEXT( '\0' );


	WIN32_FIND_DATA fd;
	HANDLE hFind = FindFirstFile( szFullPattern, &fd );
	if( hFind == INVALID_HANDLE_VALUE )
	{
		delete [] szFullPattern;
		return false;
	}
	
	do
	{
		const int iFilenameLen = ( int )_tcslen( fd.cFileName );

		TCHAR * szFullpath = new TCHAR[ uPathLen + 1 + iFilenameLen + 1 ];
		memcpy( szFullpath, szPath, uPathLen * sizeof( TCHAR ) );
		szFullpath[ uPathLen ] = TEXT( '\\' );
		memcpy( szFullpath + uPathLen + 1, fd.cFileName, iFilenameLen * sizeof( TCHAR ) );
		szFullpath[ uPathLen + 1 + iFilenameLen ] = TEXT( '\0' );


////////////////////////////////////////////////////////////////////////////////
		new PluginKind( szFullpath, bKeepLoaded );
////////////////////////////////////////////////////////////////////////////////


		delete [] szFullpath;
	}
	while( FindNextFile( hFind, &fd ) );

	FindClose( hFind );

	
	delete [] szFullPattern;
	return true;
}



#endif // PA_PLUGIN_H
