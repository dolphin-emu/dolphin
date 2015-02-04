// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Core/CoreParameter.h"
#include "InputCommon/InputConfig.h"

struct HotkeyStatus
{
	u32 button[6];
	s8  err;
};

class HotkeyManager : public ControllerEmu
{
public:
	HotkeyManager();
	~HotkeyManager();

	std::string GetName() const;
	void GetInput(HotkeyStatus* const hk);
	void LoadDefaults(const ControllerInterface& ciface);

private:
	Buttons* m_keys[6];
	ControlGroup* m_options;
};

namespace HotkeyManagerEmu
{
	void Initialize(void* const hwnd);
	void Shutdown();

	InputConfig* GetConfig();
	void GetStatus(u8 _port, HotkeyStatus* _pKeyboardStatus);
	bool IsPressed(int Id, bool held);
}
