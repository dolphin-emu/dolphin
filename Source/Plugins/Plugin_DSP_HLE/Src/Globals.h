#ifndef _GLOBALS_H
#define _GLOBALS_H


// ---------------------------------------------------------------------------------------
// wx stuff, I'm not sure if we use all these
#ifndef WX_PRECOMP
	#include <wx/wx.h>
	#include <wx/dialog.h>
#else
	#include <wx/wxprec.h>
#endif

#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/statbox.h>
#include <wx/statbmp.h>

#include <wx/sizer.h>
#include <wx/filepicker.h>
#include <wx/listctrl.h>
#include <wx/imaglist.h>
// ------------


#include "Common.h"
#include "pluginspecs_dsp.h"

extern DSPInitialize g_dspInitialize;
void DebugLog(const char* _fmt, ...);

u8 Memory_Read_U8(u32 _uAddress);
u16 Memory_Read_U16(u32 _uAddress);
u32 Memory_Read_U32(u32 _uAddress);
float Memory_Read_Float(u32 _uAddress);


#endif

