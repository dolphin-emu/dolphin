// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _GFX_DEBUGGER_H_
#define _GFX_DEBUGGER_H_

class GFXDebuggerBase
{
public:
	virtual ~GFXDebuggerBase() {}

	// if paused, debugging functions can be enabled
	virtual void OnPause() {};
	virtual void OnContinue() {};

	void DumpPixelShader(const char* path);
	void DumpVertexShader(const char* path);
	void DumpPixelShaderConstants(const char* path);
	void DumpVertexShaderConstants(const char* path);
	void DumpTextures(const char* path);
	void DumpFrameBuffer(const char* path);
	void DumpGeometry(const char* path);
	void DumpVertexDecl(const char* path);
	void DumpMatrices(const char* path);
	void DumpStats(const char* path);
};

enum PauseEvent {
	NOT_PAUSE	=	0,
	NEXT_FRAME	=	1<<0,
	NEXT_FLUSH	=	1<<1,

	NEXT_PIXEL_SHADER_CHANGE	=	1<<2,
	NEXT_VERTEX_SHADER_CHANGE	=	1<<3,
	NEXT_TEXTURE_CHANGE	=	1<<4,
	NEXT_NEW_TEXTURE	=	1<<5,

	NEXT_XFB_CMD	=	1<<6, // TODO
	NEXT_EFB_CMD	=	1<<7, // TODO

	NEXT_MATRIX_CMD	=	1<<8, // TODO
	NEXT_VERTEX_CMD	=	1<<9, // TODO
	NEXT_TEXTURE_CMD	=	1<<10, // TODO
	NEXT_LIGHT_CMD	=	1<<11, // TODO
	NEXT_FOG_CMD	=	1<<12, // TODO

	NEXT_SET_TLUT	=	1<<13, // TODO

	NEXT_ERROR	=	1<<14, // TODO
};

extern GFXDebuggerBase *g_pdebugger;
extern volatile bool GFXDebuggerPauseFlag;
extern volatile PauseEvent GFXDebuggerToPauseAtNext;
extern volatile int GFXDebuggerEventToPauseCount;
void ContinueGFXDebugger();
void GFXDebuggerCheckAndPause(bool update);
void GFXDebuggerToPause(bool update);
void GFXDebuggerUpdateScreen();

#define GFX_DEBUGGER_PAUSE_AT(event,update) {if (((GFXDebuggerToPauseAtNext & event) && --GFXDebuggerEventToPauseCount<=0) || GFXDebuggerPauseFlag) GFXDebuggerToPause(update);}
#define GFX_DEBUGGER_PAUSE_LOG_AT(event,update,dumpfunc) {if (((GFXDebuggerToPauseAtNext & event) && --GFXDebuggerEventToPauseCount<=0) || GFXDebuggerPauseFlag) {{dumpfunc};GFXDebuggerToPause(update);}}
#define GFX_DEBUGGER_LOG_AT(event,dumpfunc) {if (( GFXDebuggerToPauseAtNext & event ) ) {{dumpfunc};}}


#endif // _GFX_DEBUGGER_H_
