// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <windows.h>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace ViveInput
{

void Init(std::vector<Core::Device*>& devices);
void DeInit();

class ViveController : public Core::Device
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

	class Touch : public Core::Device::Input
	{
	public:
		Touch(u8 index, const u32& touches) : m_touches(touches), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
		u32 GetStates() const;
	private:
		const u32& m_touches;
		u8 m_index;
	};

	class Trigger : public Core::Device::Input
	{
	public:
		Trigger(u8 index, float *triggers) : m_triggers(triggers), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
	private:
		float* m_triggers;
		const u8 m_index;
	};

	class Axis : public Core::Device::Input
	{
	public:
		Axis(u8 index, s8 range, float *axes) : m_axes(axes), m_index(index), m_range(range) {}
		std::string GetName() const override;
		ControlState GetState() const override;
	private:
		float* m_axes;
		const u8 m_index;
		const s8 m_range;
	};

public:
	void UpdateInput() override;

	ViveController();

	std::string GetName() const override;
	int GetId() const override;
	std::string GetSource() const override;

private:
	u32 m_buttons, m_touches;
	float m_triggers[2], m_axes[4];
};

}
}
