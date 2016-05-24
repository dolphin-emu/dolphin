// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <windows.h>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace OculusInput
{

void Init(std::vector<Core::Device*>& devices);
void DeInit();

class OculusDevice : public Core::Device
{
private:
	class Button : public Core::Device::Input
	{
	public:
		Button(u8 index, const u32& buttons) : m_buttons(buttons), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
		u32 GetStates() const;
	private:
		const u32& m_buttons;
		u8 m_index;
	};

public:
	void UpdateInput() override;

	OculusDevice();
	~OculusDevice();

	std::string GetName() const override;
	int GetId() const override;
	std::string GetSource() const override;

private:
	u32 m_state;
};


}
}
