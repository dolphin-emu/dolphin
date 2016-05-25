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

class SIDRemote : public Core::Device
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

	SIDRemote();

	std::string GetName() const override;
	int GetId() const override;
	std::string GetSource() const override;

private:
	u32 m_buttons;
};

class OculusTouch : public Core::Device
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
		Trigger(u8 index, const BYTE& trigger, BYTE range) : m_trigger(trigger), m_range(range), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
	private:
		const BYTE& m_trigger;
		const BYTE m_range;
		const u8 m_index;
	};

	class Axis : public Core::Device::Input
	{
	public:
		Axis(u8 index, const SHORT& axis, SHORT range) : m_axis(axis), m_range(range), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
	private:
		const SHORT& m_axis;
		const SHORT m_range;
		const u8 m_index;
	};

public:
	void UpdateInput() override;

	OculusTouch();

	std::string GetName() const override;
	int GetId() const override;
	std::string GetSource() const override;

private:
	u32 m_buttons, m_touches;
};

class HMDDevice : public Core::Device
{
private:
	class Gesture : public Core::Device::Input
	{
	public:
		Gesture(u8 index, const u32& gestures) : m_gestures(gestures), m_index(index) {}
		std::string GetName() const override;
		ControlState GetState() const override;
		u32 GetStates() const;
	private:
		const u32& m_gestures;
		u8 m_index;
	};

public:
	void UpdateInput() override;

	HMDDevice();

	std::string GetName() const override;
	int GetId() const override;
	std::string GetSource() const override;

private:
	u32 m_gestures;
};


}
}
