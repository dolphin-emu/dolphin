
#ifndef _WIIMOTE_H_
#define _WIIMOTE_H_

#include "../../InputCommon/Src/InputConfig.h"

#define MAX_WIIMOTES	4

#define WIIMOTE_INI_NAME	"WiimoteNew"

enum
{
	WIIMOTE_SRC_NONE = 0,
	WIIMOTE_SRC_EMU = 1,
	WIIMOTE_SRC_REAL = 2,
	WIIMOTE_SRC_HYBRID = 3,	// emu + real
};

extern unsigned int g_wiimote_sources[MAX_WIIMOTES];

namespace Wiimote
{

void Shutdown();
void Initialize(void* const hwnd);

unsigned int GetAttached();
void DoState(unsigned char **ptr, int mode);
void EmuStateChange(EMUSTATE_CHANGE newState);
InputPlugin *GetPlugin();

void ControlChannel(int _number, u16 _channelID, const void* _pData, u32 _Size);
void InterruptChannel(int _number, u16 _channelID, const void* _pData, u32 _Size);
void Update(int _number);

}

namespace WiimoteReal
{

unsigned int Initialize();
void Shutdown();
void Refresh();

void LoadSettings();

#ifdef _WIN32
int PairUp(bool unpair = false);
int UnPair();
#endif

}

#endif
