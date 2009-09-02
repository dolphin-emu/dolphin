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

#ifndef _DX_DEBUGGER_H_
#define _DX_DEBUGGER_H_

#include <wx/wx.h>
#include <wx/notebook.h>

#include "../Globals.h"

class IniFile;

class GFXDebuggerDX9 : public wxDialog
{
public:
	GFXDebuggerDX9(wxWindow *parent,
		wxWindowID id = 1,
		const wxString &title = wxT("Video"),
		const wxPoint& pos = wxDefaultPosition,
		const wxSize& size = wxDefaultSize,
		#ifdef _WIN32
		long style = wxNO_BORDER);
		#else
		long style = wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN | wxNO_FULL_REPAINT_ON_RESIZE);
		#endif

	virtual ~GFXDebuggerDX9();

	void SaveSettings() const;
	void LoadSettings();

	bool bInfoLog;
	bool bPrimLog;
	bool bSaveTextures;
	bool bSaveTargets;
	bool bSaveShaders;

private:
	DECLARE_EVENT_TABLE();

	wxPanel *m_MainPanel;

	wxCheckBox	*m_Check[6];
	wxButton	*m_pButtonPause;
	wxButton	*m_pButtonPauseAtNext;
	wxButton	*m_pButtonGo;
	wxChoice	*m_pPauseAtList;
	wxButton	*m_pButtonDump;
	wxChoice	*m_pDumpList;
	wxButton	*m_pButtonUpdateScreen;
	wxButton	*m_pButtonClearScreen;
	wxTextCtrl	*m_pCount;


	// WARNING: Make sure these are not also elsewhere
	enum
	{
		ID_MAINPANEL = 3900,
		ID_SAVETOFILE,
		ID_INFOLOG,
		ID_PRIMLOG,
		ID_SAVETEXTURES,
		ID_SAVETARGETS,
		ID_SAVESHADERS,
		NUM_OPTIONS,
		ID_GO,
		ID_PAUSE,
		ID_PAUSE_AT_NEXT,
		ID_PAUSE_AT_LIST,
		ID_DUMP,
		ID_DUMP_LIST,
		ID_UPDATE_SCREEN,
		ID_CLEAR_SCREEN,
		ID_COUNT
	};

	void OnClose(wxCloseEvent& event);		
	void CreateGUIControls();

	void GeneralSettings(wxCommandEvent& event);
	void OnPauseButton(wxCommandEvent& event);
	void OnPauseAtNextButton(wxCommandEvent& event);
	void OnDumpButton(wxCommandEvent& event);
	void OnGoButton(wxCommandEvent& event);
	void OnUpdateScreenButton(wxCommandEvent& event);
	void OnClearScreenButton(wxCommandEvent& event);
	void OnCountEnter(wxCommandEvent& event);
};

enum PauseEvent {
	NEXT_FRAME,
	NEXT_FLUSH,
	NEXT_FIFO,
	NEXT_DLIST,
	NEXT_PIXEL_SHADER_CHANGE,
	NEXT_VERTEX_SHADER_CHANGE,
	NEXT_NEW_TEXTURE,
	NEXT_RENDER_TEXTURE,
	NEXT_MATRIX_CMD,
	NEXT_VERTEX_CMD,
	NEXT_TEXTURE_CMD,
	NEXT_LIGHT_CMD,
	NEXT_FRAME_BUFFER_CMD,
	NEXT_FOG_CMD,
	NEXT_SET_CONSTANT_COLOR,
	NEXT_UCODE,
	NEXT_SET_TEXTURE,
	NEXT_SET_LIGHT,
	NEXT_SET_MODE_CMD,
	NEXT_UNKNOWN_OP,
	NEXT_LOADTLUT,
	NEXT_SWITCH_UCODE,
	NOT_PAUSE,
};

extern volatile bool DX9DebuggerPauseFlag;
extern volatile PauseEvent DX9DebuggerToPauseAtNext;
extern volatile int DX9DebuggerEventToPauseCount;
void ContinueDX9Debugger();
void DX9DebuggerCheckAndPause();

#undef ENABLE_DX_DEBUGGER
#ifdef _DEBUG 
#define ENABLE_DX_DEBUGGER
#else
#ifdef DEBUGFAST
#define ENABLE_DX_DEBUGGER
#endif
#endif

#ifdef ENABLE_DX_DEBUGGER

#define DX9DEBUGGER_PAUSE {DX9DebuggerCheckAndPause();}
#define DX9DEBUGGER_PAUSE_IF(op)	if(DX9DebuggerToPauseAtNext == op  && DX9DebuggerToPauseAtNext!=NOT_PAUSE){DX9DebuggerToPauseAtNext = NOT_PAUSE;DX9DebuggerPauseFlag = true;DX9DebuggerCheckAndPause();}
#define DEBUGGER_PAUSE_COUNT_N(PauseEvent) DX9Debugger_Pause_Count_N(PauseEvent,true)
#define DEBUGGER_PAUSE_COUNT_N_WITHOUT_UPDATE(PauseEvent) DX9Debugger_Pause_Count_N(PauseEvent,false)

extern void DX9Debugger_Pause_Count_N(PauseEvent,bool);

//#define DebuggerPauseCountN DEBUGGER_PAUSE_COUNT_N

//#define DEBUGGER_PAUSE_AND_DUMP(op,dumpfuc)		\
//	if(pauseAtNext && eventToPause == op)	\
//	{	pauseAtNext = false;debuggerPause = true; CGraphicsContext::Get()->UpdateFrame(); dumpfuc;}
//#define DEBUGGER_PAUSE_AND_DUMP_NO_UPDATE(op,dumpfuc)		\
//	if(pauseAtNext && eventToPause == op)	\
//	{	pauseAtNext = false;debuggerPause = true; dumpfuc;}
//
//#define DEBUGGER_PAUSE_AND_DUMP_COUNT_N(op,dumpfuc)		\
//	if(pauseAtNext && eventToPause == op)	\
//{	if( debuggerPauseCount > 0 ) debuggerPauseCount--; if( debuggerPauseCount == 0 ){pauseAtNext = false;debuggerPause = true; CGraphicsContext::Get()->UpdateFrame(); dumpfuc;}}
//
//#define DEBUGGER_PAUSE_AT_COND_AND_DUMP_COUNT_N(cond,dumpfuc)		\
//	if(pauseAtNext && (cond) )	\
//{	if( debuggerPauseCount > 0 ) debuggerPauseCount--; if( debuggerPauseCount == 0 ){pauseAtNext = false;debuggerPause = true; CGraphicsContext::Get()->UpdateFrame(); dumpfuc;}}
//
//void RDP_NOIMPL_Real(LPCTSTR op,u32,u32) ;
//#define RSP_RDP_NOIMPL RDP_NOIMPL_Real
//#define DEBUGGER_IF_DUMP(cond, dumpfunc)	{if(cond) {dumpfunc}}
//#define TXTRBUF_DUMP(dumpfunc)			DEBUGGER_IF_DUMP((logTextureBuffer), dumpfunc)
//#define TXTRBUF_DETAIL_DUMP(dumpfunc)			DEBUGGER_IF_DUMP((logTextureBuffer&&logDetails), dumpfunc)
//#define TXTRBUF_OR_CI_DUMP(dumpfunc)	DEBUGGER_IF_DUMP((logTextureBuffer || (pauseAtNext && eventToPause == NEXT_SET_CIMG)), dumpfunc)
//#define TXTRBUF_OR_CI_DETAIL_DUMP(dumpfunc)	DEBUGGER_IF_DUMP(((logTextureBuffer || (pauseAtNext && eventToPause == NEXT_SET_CIMG))&&logDetails), dumpfunc)
//#define VTX_DUMP(dumpfunc)			DEBUGGER_IF_DUMP((logVertex && pauseAtNext), dumpfunc)
//#define TRI_DUMP(dumpfunc)			DEBUGGER_IF_DUMP((logTriangles && pauseAtNext), dumpfunc)
//#define LIGHT_DUMP(dumpfunc)		DEBUGGER_IF_DUMP((eventToPause == NEXT_SET_LIGHT && pauseAtNext), dumpfunc)
//#define WARNING(dumpfunc)			DEBUGGER_IF_DUMP(logWarning, dumpfunc)
//#define FOG_DUMP(dumpfunc)			DEBUGGER_IF_DUMP(logFog, dumpfunc)
//#define LOG_TEXTURE(dumpfunc)			DEBUGGER_IF_DUMP((logTextures || (pauseAtNext && eventToPause==NEXT_TEXTURE_CMD) ), dumpfunc)
//#define DEBUGGER_ONLY_IF	DEBUGGER_IF_DUMP
//#define DEBUGGER_ONLY(func)	{func}
//
//#define TRACE0(arg0)			{DebuggerAppendMsg(arg0);}
//#define TRACE1(arg0,arg1)		{DebuggerAppendMsg(arg0,arg1);}
//#define TRACE2(arg0,arg1,arg2)	{DebuggerAppendMsg(arg0,arg1,arg2);}
//#define TRACE3(arg0,arg1,arg2,arg3)	{DebuggerAppendMsg(arg0,arg1,arg2,arg3);}
//#define TRACE4(arg0,arg1,arg2,arg3,arg4)	{DebuggerAppendMsg(arg0,arg1,arg2,arg3,arg4);}
//#define TRACE5(arg0,arg1,arg2,arg3,arg4,arg5)	{DebuggerAppendMsg(arg0,arg1,arg2,arg3,arg4,arg5);}

//#define DEBUG_TRIANGLE(dumpfunc) { if(pauseAtNext && eventToPause==NEXT_TRIANGLE ) { eventToPause = NEXT_FLUSH_TRI; debuggerPause = true; DEBUGGER_PAUSE(NEXT_FLUSH_TRI); dumpfunc} }

#else
// Not to use debugger in release build
#define DX9DEBUGGER_PAUSE
#define DX9DEBUGGER_PAUSE_IF(op)
#define DEBUGGER_PAUSE(op)
#define DEBUGGER_PAUSE_COUNT_N(op)
#define DEBUGGER_PAUSE_COUNT_N_WITHOUT_UPDATE(PauseEvent)

//#define DEBUG_DUMP_VERTEXES(str, v0, v1, v2)
//#define DEBUGGER_IF(op)
//#define DEBUGGER_PAUSE_AND_DUMP(op,dumpfuc)
//#define DebuggerPauseCountN DEBUGGER_PAUSE_COUNT_N
//#define DEBUGGER_PAUSE_AT_COND_AND_DUMP_COUNT_N(cond,dumpfuc)
//#define DEBUGGER_PAUSE_AND_DUMP_COUNT_N(op,dumpfuc)	
//#define DEBUGGER_PAUSE_COUNT_N_WITHOUT_UPDATE(op)
//#define DEBUGGER_PAUSE_AND_DUMP_NO_UPDATE(op,dumpfuc)
//#define RSP_RDP_NOIMPL(a,b,c)
//void __cdecl DebuggerAppendMsg(const char * Message, ...);
//#define DumpHex(rdramAddr, count)	
//#define DEBUGGER_IF_DUMP(cond, dumpfunc)
//#define TXTRBUF_DUMP(dumpfunc)
//#define TXTRBUF_DETAIL_DUMP(dumpfunc)
//#define TXTRBUF_OR_CI_DUMP(dumpfunc)
//#define TXTRBUF_OR_CI_DETAIL_DUMP(dumpfunc)
//#define VTX_DUMP(dumpfunc)
//#define TRI_DUMP(dumpfunc)
//#define LIGHT_DUMP(dumpfunc)
//#define WARNING(dumpfunc)
//#define FOG_DUMP(dumpfunc)
//#define LOG_TEXTURE(dumpfunc)
//#define DEBUGGER_ONLY_IF	DEBUGGER_IF_DUMP
//#define DEBUGGER_ONLY(func)
//#define DumpMatrix(a,b)
//
//#define TRACE0(arg0)		{}
//#define TRACE1(arg0,arg1)	{}
//#define TRACE2(arg0,arg1,arg2)		{}
//#define TRACE3(arg0,arg1,arg2,arg3)	{}
//#define TRACE4(arg0,arg1,arg2,arg3,arg4)		{}
//#define TRACE5(arg0,arg1,arg2,arg3,arg4,arg5)	{}
//
//#define DEBUG_TRIANGLE(arg0)	{}

#endif ENABLE_DX_DEBUGGER


#endif // _DX_DEBUGGER_H_
