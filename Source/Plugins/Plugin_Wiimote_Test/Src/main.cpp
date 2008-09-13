#include <wx/aboutdlg.h>

#include "pluginspecs_wiimote.h"

SWiimoteInitialize g_WiimoteInitialize;
#define VERSION_STRING "0.1"


void GetDllInfo (PLUGIN_INFO* _PluginInfo) 
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_WIIMOTE;
#ifdef DEBUGFAST 
	sprintf(_PluginInfo->Name, "Wiimote Test (DebugFast) " VERSION_STRING);
#else
#ifndef _DEBUG
	sprintf(_PluginInfo->Name, "Wiimote Test " VERSION_STRING);
#else
	sprintf(_PluginInfo->Name, "Wiimote Test (Debug) " VERSION_STRING);
#endif
#endif
}


void DllAbout(HWND _hParent) 
{
	wxAboutDialogInfo info;
	info.AddDeveloper(_T("masken (masken3@gmail.com)"));
	info.SetDescription(_T("Wiimote test plugin"));
	wxAboutBox(info);
}

void DllConfig(HWND _hParent)
{
}


void Wiimote_Initialize(SWiimoteInitialize* _pWiimoteInitialize)
{
	if (_pWiimoteInitialize == NULL)
		return;

	g_WiimoteInitialize = *_pWiimoteInitialize;
}

void Wiimote_DoState(unsigned char **ptr, int mode) {
}

void Wiimote_Shutdown(void) 
{
}

void Wiimote_Output(const void* _pData, u32 _Size) {
}

unsigned int Wiimote_GetAttachedControllers() {
	return 1;
}
