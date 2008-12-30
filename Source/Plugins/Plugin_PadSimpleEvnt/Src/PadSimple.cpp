// Copyright (C) 2003-2008 Dolphin Project.

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

#include <stdio.h>
#include <math.h>

#include "Common.h"
#include "pluginspecs_pad.h"
#include "PadSimple.h"
#include "IniFile.h"

#if defined(HAVE_WX) && HAVE_WX
#include "GUI/ConfigDlg.h"
#endif

SPads pad[4];
bool KeyStatus[NUMCONTROLS];

HINSTANCE g_hInstance;
SPADInitialize g_PADInitialize;

#define RECORD_SIZE (1024 * 128)
SPADStatus recordBuffer[RECORD_SIZE];
int count = 0;

void RecordInput(const SPADStatus& _rPADStatus)
{
	if (count >= RECORD_SIZE)
	{
		return;
	}

	recordBuffer[count++] = _rPADStatus;
}


const SPADStatus& PlayRecord()
{
	if (count >= RECORD_SIZE){return(recordBuffer[0]);}

	return(recordBuffer[count++]);
}


void LoadRecord()
{
	FILE* pStream = fopen("c:\\pad-record.bin", "rb");

	if (pStream != NULL)
	{
		fread(recordBuffer, 1, RECORD_SIZE * sizeof(SPADStatus), pStream);
		fclose(pStream);
	}
}


void SaveRecord()
{
	FILE* pStream = fopen("c:\\pad-record.bin", "wb");

	if (pStream != NULL)
	{
		fwrite(recordBuffer, 1, RECORD_SIZE * sizeof(SPADStatus), pStream);
		fclose(pStream);
	}
}


#ifdef _WIN32

class wxDLLApp : public wxApp
{
	bool OnInit()
	{
		return true;
	}
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp)

WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);

BOOL APIENTRY DllMain(HINSTANCE hinstDLL,	// DLL module handle
					  DWORD dwReason,		// reason called
					  LPVOID lpvReserved)	// reserved
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		{       //use wxInitialize() if you don't want GUI instead of the following 12 lines
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);
			if ( !wxTheApp || !wxTheApp->CallOnInit() )
				return FALSE;
		}
		break; 

	case DLL_PROCESS_DETACH:
		wxEntryCleanup(); //use wxUninitialize() if you don't want GUI 
		break;
	default:
		break;
	}

	g_hInstance = hinstDLL;
	return(TRUE);
}

#endif


void GetDllInfo(PLUGIN_INFO* _PluginInfo)
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_PAD;

#ifdef DEBUGFAST 
	sprintf(_PluginInfo->Name, "Dolphin event pad (DebugFast)");
#elif defined _DEBUG
	sprintf(_PluginInfo->Name, "Dolphin event pad (Debug)");
#else
	sprintf(_PluginInfo->Name, "Dolphin event pad");
#endif

}

void DllConfig(HWND _hParent)
{
	LoadConfig();
	ConfigDialog frame(NULL);
	frame.ShowModal();
	SaveConfig();
}

void DllDebugger(HWND _hParent, bool Show) {
}

void PAD_Initialize(SPADInitialize _PADInitialize)
{
#ifdef RECORD_REPLAY
	LoadRecord();
#endif

	LoadConfig();
}


void PAD_Shutdown()
{
#ifdef RECORD_STORE
	SaveRecord();
#endif
	SaveConfig();
}


void ParseKeyEvent(SPADStatus* _pPADStatus)
{
    int stickvalue = (KeyStatus[CTL_HALFPRESS]) ? 40 : 100;
    int triggervalue = (KeyStatus[CTL_HALFPRESS]) ? 100 : 255;

    if (KeyStatus[CTL_MAINLEFT]){_pPADStatus->stickX -= stickvalue;}
    if (KeyStatus[CTL_MAINUP]){_pPADStatus->stickY += stickvalue;}
    if (KeyStatus[CTL_MAINRIGHT]){_pPADStatus->stickX += stickvalue;}
    if (KeyStatus[CTL_MAINDOWN]){_pPADStatus->stickY -= stickvalue;}

    if (KeyStatus[CTL_SUBLEFT]){_pPADStatus->substickX -= stickvalue;}
    if (KeyStatus[CTL_SUBUP]){_pPADStatus->substickY += stickvalue;}
    if (KeyStatus[CTL_SUBRIGHT]){_pPADStatus->substickX += stickvalue;}
    if (KeyStatus[CTL_SUBDOWN]){_pPADStatus->substickY -= stickvalue;}

    if (KeyStatus[CTL_DPADLEFT]){_pPADStatus->button |= PAD_BUTTON_LEFT;}
    if (KeyStatus[CTL_DPADUP]){_pPADStatus->button |= PAD_BUTTON_UP;}
    if (KeyStatus[CTL_DPADRIGHT]){_pPADStatus->button |= PAD_BUTTON_RIGHT;}
    if (KeyStatus[CTL_DPADDOWN]){_pPADStatus->button |= PAD_BUTTON_DOWN;}

    if (KeyStatus[CTL_A]) {
        _pPADStatus->button |= PAD_BUTTON_A;
        _pPADStatus->analogA = 255;
    }
    
    if (KeyStatus[CTL_B]) {
        _pPADStatus->button |= PAD_BUTTON_B;
        _pPADStatus->analogB = 255;
    }
    
    if (KeyStatus[CTL_X]){_pPADStatus->button |= PAD_BUTTON_X;}
    if (KeyStatus[CTL_Y]){_pPADStatus->button |= PAD_BUTTON_Y;}
    if (KeyStatus[CTL_Z]){_pPADStatus->button |= PAD_TRIGGER_Z;}
	
    if (KeyStatus[CTL_L]) {
        _pPADStatus->button |= PAD_TRIGGER_L;
        _pPADStatus->triggerLeft = triggervalue;
    }
    
    if (KeyStatus[CTL_R]) {
        _pPADStatus->button |= PAD_TRIGGER_R;
        _pPADStatus->triggerRight = triggervalue;
    }
    
    if (KeyStatus[CTL_START]){_pPADStatus->button |= PAD_BUTTON_START;}
    if (KeyStatus[CTL_MIC])
    	_pPADStatus->MicButton = true;
    else
    	_pPADStatus->MicButton = false;
}


void PAD_GetStatus(u8 _numPAD, SPADStatus* _pPADStatus)
{
	// Check if all is okay
	if ((_pPADStatus == NULL))
	{
		return;
	}

#ifdef RECORD_REPLAY
	*_pPADStatus = PlayRecord();
	return;
#endif

	const int base = 0x80;
	// Clear pad
	memset(_pPADStatus, 0, sizeof(SPADStatus));

	_pPADStatus->stickY = base;
	_pPADStatus->stickX = base;
	_pPADStatus->substickX = base;
	_pPADStatus->substickY = base;
	_pPADStatus->button |= PAD_USE_ORIGIN;

	_pPADStatus->err = PAD_ERR_NONE;

#ifdef RECORD_STORE
	RecordInput(*_pPADStatus);
#endif
}


unsigned int PAD_GetAttachedPads()
{
	unsigned int connected = 0;
	
	LoadConfig();

	if(pad[0].bAttached)
		connected |= 1;		
	if(pad[1].bAttached)
		connected |= 2;
	if(pad[2].bAttached)
		connected |= 4;
	if(pad[3].bAttached)
		connected |= 8;

	return connected;
}


void LoadConfig()
{
    const int defaultKeyForControl[NUMCONTROLS] = {
          sf::Key::X, //A
          sf::Key::Z,
          sf::Key::S,
          sf::Key::C,
          sf::Key::D,
          sf::Key::Return,
          sf::Key::Q,
          sf::Key::W,
          sf::Key::Up, //mainstick
          sf::Key::Down,
          sf::Key::Left, 
          sf::Key::Right,
          sf::Key::I, //substick
          sf::Key::K,
          sf::Key::J,
          sf::Key::L,
          sf::Key::T, //dpad
          sf::Key::G,
          sf::Key::F,
          sf::Key::H,
	  sf::Key::LShift, //halfpress
	  sf::Key::P
	};

	IniFile file;
	file.Load(FULL_CONFIG_DIR "pad.ini");

	for(int i = 0; i < 4; i++) {
	    char SectionName[32];
	    sprintf(SectionName, "PAD%i", i+1);
	    
	    file.Get(SectionName, "Attached", &pad[i].bAttached, i==0);
	    file.Get(SectionName, "DisableOnBackground", &pad[i].bDisable, false);
	    for (int x = 0; x < NUMCONTROLS; x++) {
		file.Get(SectionName, controlNames[x], 
			 &pad[i].keyForControl[x], 
			 (i==0)?defaultKeyForControl[x]:0);
	    }
	}
}


void SaveConfig()
{
	IniFile file;
	file.Load(FULL_CONFIG_DIR "pad.ini");

	for(int i = 0; i < 4; i++)
	{
		char SectionName[32];
		sprintf(SectionName, "PAD%i", i+1);

		file.Set(SectionName, "Attached", pad[i].bAttached);
		file.Set(SectionName, "DisableOnBackground", pad[i].bDisable);
		
		for (int x = 0; x < NUMCONTROLS; x++)
		{
			file.Set(SectionName, controlNames[x], pad[i].keyForControl[x]);
		}
	}
	file.Save(FULL_CONFIG_DIR "pad.ini");
}


