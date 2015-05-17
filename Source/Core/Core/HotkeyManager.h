// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
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
	Buttons* m_keys[3];
	ControlGroup* m_options;
};

namespace HotkeyManagerEmu
{
	void Initialize(void* const hwnd);
	void Shutdown();
	void LoadConfig();

	InputConfig* GetConfig();
	void GetStatus();
	bool IsEnabled();
	void Enable(bool enable_toggle);
	bool IsPressed(int Id, bool held);
}
