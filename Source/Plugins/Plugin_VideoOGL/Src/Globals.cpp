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


#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/notebook.h>
#include <wx/dialog.h>
#include <wx/aboutdlg.h>

#include "Globals.h"

#include "pluginspecs_video.h"
#include "main.h"

#include "IniFile.h"
#include <assert.h>

float MValueX, MValueY; // Since it can Stretch to fit Window, we need two different multiplication values//
int frameCount;
Config g_Config;

Statistics stats;

void Statistics::ResetFrame()
{
	memset(&thisFrame, 0, sizeof(ThisFrame));
}

Config::Config()
{
    memset(this, 0, sizeof(Config));
}

void Config::Load()
{
    std::string temp;
    IniFile iniFile;
    iniFile.Load("gfx_opengl.ini");

    iniFile.Get("Hardware", "Adapter", &iAdapter, 0);
	if (iAdapter == -1) 
        iAdapter = 0;

	// get resolution
    iniFile.Get("Hardware", "WindowedRes", &temp, 0);
	if(temp.empty())
		temp = "640x480";
    strcpy(iWindowedRes, temp.c_str());
    iniFile.Get("Hardware", "FullscreenRes", &temp, 0);
	if(temp.empty())
		temp = "640x480";
    strcpy(iFSResolution, temp.c_str());

    iniFile.Get("Hardware", "Fullscreen", &bFullscreen, 0); // Hardware
	iniFile.Get("Hardware", "RenderToMainframe", &renderToMainframe, 0);

    iniFile.Get("Settings", "ShowFPS", &bShowFPS, false); // Settings
	iniFile.Get("Settings", "OverlayStats", &bOverlayStats, false);
    iniFile.Get("Settings", "Postprocess", &iPostprocessEffect, 0);
    iniFile.Get("Settings", "DLOptimize", &iCompileDLsLevel, 0);
    iniFile.Get("Settings", "DumpTextures", &bDumpTextures, 0);
    iniFile.Get("Settings", "ShowShaderErrors", &bShowShaderErrors, 0);
	iniFile.Get("Settings", "LogLevel", &iLog, 0);
    iniFile.Get("Settings", "Multisample", &iMultisampleMode, 0);
    if(iMultisampleMode == 0)
        iMultisampleMode = 1;
	std::string s;
    iniFile.Get("Settings", "TexDumpPath", &s, 0);
    if( s.size() < sizeof(texDumpPath) )
        strcpy(texDumpPath, s.c_str());
    else {
        strncpy(texDumpPath, s.c_str(), sizeof(texDumpPath)-1);
        texDumpPath[sizeof(texDumpPath)-1] = 0;
    }

	iniFile.Get("Settings", "TexFmtOverlayEnable", &bTexFmtOverlayEnable, 0);
	iniFile.Get("Settings", "TexFmtOverlayCenter", &bTexFmtOverlayCenter, 0);
    
    iniFile.Get("Enhancements", "ForceFiltering", &bForceFiltering, 0);
    iniFile.Get("Enhancements", "ForceMaxAniso", &bForceMaxAniso, 0);
	iniFile.Get("Enhancements", "StretchToFit", &bStretchToFit, false);
	iniFile.Get("Enhancements", "KeepAR", &bKeepAR, false);
}

void Config::Save()
{
    IniFile iniFile;
    iniFile.Load("gfx_opengl.ini");
    iniFile.Set("Hardware", "Adapter", iAdapter);
    iniFile.Set("Hardware", "WindowedRes", iWindowedRes);
    iniFile.Set("Hardware", "FullscreenRes", iFSResolution);
    iniFile.Set("Hardware", "Fullscreen", bFullscreen);
	iniFile.Set("Hardware", "RenderToMainframe", renderToMainframe);	

    iniFile.Set("Settings", "ShowFPS", bShowFPS);
	iniFile.Set("Settings", "OverlayStats", bOverlayStats);
    iniFile.Set("Settings", "Postprocess", iPostprocessEffect);
    iniFile.Set("Settings", "DLOptimize", iCompileDLsLevel);
    iniFile.Set("Settings", "DumpTextures", bDumpTextures);
    iniFile.Set("Settings", "ShowShaderErrors", bShowShaderErrors);
	iniFile.Set("Settings", "LogLevel", iLog);
    iniFile.Set("Settings", "Multisample", iMultisampleMode);
    iniFile.Set("Settings", "TexDumpPath", texDumpPath);
	iniFile.Set("Settings", "TexFmtOverlayEnable", bTexFmtOverlayEnable);
	iniFile.Set("Settings", "TexFmtOverlayCenter", bTexFmtOverlayCenter);

    iniFile.Set("Enhancements", "ForceFiltering", bForceFiltering);
    iniFile.Set("Enhancements", "ForceMaxAniso", bForceMaxAniso);
	iniFile.Set("Enhancements", "StretchToFit", bStretchToFit);
	iniFile.Set("Enhancements", "KeepAR", bKeepAR);

    iniFile.Save("gfx_opengl.ini");
}

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif

struct TGA_HEADER
{
    u8  identsize;          // size of ID field that follows 18 u8 header (0 usually)
    u8  colourmaptype;      // type of colour map 0=none, 1=has palette
    u8  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

    s16 colourmapstart;     // first colour map entry in palette
    s16 colourmaplength;    // number of colours in palette
    u8  colourmapbits;      // number of bits per palette entry 15,16,24,32

    s16 xstart;             // image x origin
    s16 ystart;             // image y origin
    s16 width;              // image width in pixels
    s16 height;             // image height in pixels
    u8  bits;               // image bits per pixel 8,16,24,32
    u8  descriptor;         // image descriptor bits (vh flip bits)
    
    // pixel data follows header
    

};
#if defined(_MSC_VER)
#pragma pack(pop)
#endif

bool SaveTGA(const char* filename, int width, int height, void* pdata)
{
    TGA_HEADER hdr;
    FILE* f = fopen(filename, "wb");
    if (f == NULL)
        return false;

    _assert_( sizeof(TGA_HEADER) == 18 && sizeof(hdr) == 18 );
    
    memset(&hdr, 0, sizeof(hdr));
    hdr.imagetype = 2;
    hdr.bits = 32;
    hdr.width = width;
    hdr.height = height;
    hdr.descriptor |= 8|(1<<5); // 8bit alpha, flip vertical

    fwrite(&hdr, sizeof(hdr), 1, f);
    fwrite(pdata, width*height*4, 1, f);
    fclose(f);
    return true;
}

bool SaveTexture(const char* filename, u32 textarget, u32 tex, int width, int height)
{
    GL_REPORT_ERRORD();
	std::vector<u32> data(width*height);
    glBindTexture(textarget, tex);
    glGetTexImage(textarget, 0, GL_BGRA, GL_UNSIGNED_BYTE, &data[0]);
    GLenum err;
    GL_REPORT_ERROR();
    if (err != GL_NO_ERROR) {
        return false;
    }

    return SaveTGA(filename, width, height, &data[0]);
}

bool SaveData(const char* filename, const char* data)
{
    FILE* f = fopen(filename, "wb");
    if (f == NULL)
        return false;

    fwrite(data, strlen(data), 1, f);
    fclose(f);
    return true;
}


#ifdef _WIN32
// The one for Linux is in Linux/Linux.cpp
static HANDLE hConsole = NULL;
void OpenConsole() {
	COORD csize;
	CONSOLE_SCREEN_BUFFER_INFO csbiInfo; 
	SMALL_RECT srect;

	if (hConsole) return;
	AllocConsole();
	SetConsoleTitle("Opengl Plugin Output");

	// set width and height
	csize.X = 155; // this fits on 1280 pixels TODO: make it adjustable from the wx debugging window
	csize.Y = 1024;
	SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), csize);

	// make the internal buffer match the width we set
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo);
	srect = csbiInfo.srWindow;
	srect.Right = srect.Left + csize.X - 1; // match
	srect.Bottom = srect.Top + 44;
	SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &srect);

	hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
}

void CloseConsole() {
	if (hConsole == NULL) return;
	FreeConsole(); hConsole = NULL;
}
#endif



static FILE* pfLog = NULL;
void __Log(const char *fmt, ...)
{

    char* Msg = (char*)alloca(strlen(fmt)+512);
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( Msg, strlen(fmt)+512, fmt, ap );
    va_end( ap );

    g_VideoInitialize.pLog(Msg, FALSE);

    if( pfLog == NULL ) pfLog = fopen("Logs/oglgfx.txt", "w");

    if( pfLog != NULL )
        fwrite(Msg, strlen(Msg), 1, pfLog);
#ifdef _WIN32
    DWORD tmp;
    WriteConsole(hConsole, Msg, (DWORD)strlen(Msg), &tmp, 0);
#else
	//printf("%s", Msg);
#endif

}

void __Log(int type, const char *fmt, ...)
{

    char* Msg = (char*)alloca(strlen(fmt)+512);
    va_list ap;

    va_start( ap, fmt );
    vsnprintf( Msg, strlen(fmt)+512, fmt, ap );
    va_end( ap );

    g_VideoInitialize.pLog(Msg, FALSE);

#ifdef _WIN32
    DWORD tmp;
    WriteConsole(hConsole, Msg, (DWORD)strlen(Msg), &tmp, 0);
#endif

}
