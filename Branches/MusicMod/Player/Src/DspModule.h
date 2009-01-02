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


#ifndef PA_DSP_MODULE_H
#define PA_DSP_MODULE_H



#include "Global.h"
#include "DspPlugin.h"
#include "Winamp/Dsp.h"



class DspModule;
class DspPlugin;

extern DspModule ** active_dsp_mods;
extern int active_dsp_count;



////////////////////////////////////////////////////////////////////////////////
/// Winamp DSP module wrapper
////////////////////////////////////////////////////////////////////////////////
class DspModule
{
public:
	inline bool IsActive() { return ( iArrayIndex != -1 ); }
	
	inline TCHAR * GetName()  { return szName;   }
	inline int GetNameLen()   { return iNameLen; }

	DspModule( char * szName, int iIndex, winampDSPModule * mod, DspPlugin * plugin );
//	DspModule( wchar_t * szName, int iIndex, winampVisModule * mod, VisPlugin * plugin );
	
	bool Start( int iIndex );
	bool Stop();
	
	bool Config();
	
private:
	int      iArrayIndex;

	TCHAR *  szName;
	int      iNameLen;

	int                iIndex;
	winampDSPModule *  mod;
	DspPlugin *        plugin;


	friend int dsp_dosamples( short int * samples, int numsamples, int bps, int nch, int srate );
};



#endif // PA_DSP_MODULE_H
