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


#ifndef PA_VIS_MODULE_H
#define PA_VIS_MODULE_H



#include "Global.h"
#include "VisPlugin.h"
#include <Process.h>



class VisModule;
class VisPlugin;

extern VisModule ** active_vis_mods;
extern int active_vis_count;



////////////////////////////////////////////////////////////////////////////////
/// Winamp visualization module wrapper
////////////////////////////////////////////////////////////////////////////////
class VisModule
{
public:
	VisModule( char * szName, int iIndex, winampVisModule * mod, VisPlugin * plugin );
	
	bool Start();
	bool Config();
	bool Stop();
	
	inline bool IsActive() { return bActive; }

	inline TCHAR *  GetName()     { return szName;   }
	inline int      GetNameLen()  { return iNameLen; }

private:
	int      iArrayIndex;
	bool     bActive;
	bool     bShouldQuit;
	bool     bAllowRender;

	TCHAR *  szName;
	int      iNameLen;

	int                iIndex;
	winampVisModule *  mod;
	VisPlugin *        plugin;

	
	friend void PlugThread( PVOID pvoid );
	friend void VSAAdd( void * data, int timestamp );
	friend void VSAAddPCMData( void * PCMData, int nch, int bps, int timestamp );
	friend int VSAGetMode( int * specNch, int * waveNch );
};



#endif // PA_VIS_MODULE_H
