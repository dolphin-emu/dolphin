// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <osvr/ClientKit/ClientKitC.h>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace OSVR
{

#define OSVR_NUM_BUTTONS 14
#define OSVR_NUM_AXES 4
#define OSVR_NUM_TRIGGERS 2

void Init(std::vector<Core::Device*>& devices);
void DeInit();

class Tracker : public Core::Device
{
private:
	class Orientation : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		bool IsDetectable() { return false; }
		Orientation(u8 index, const double& axis) : m_index(index), m_axis(axis) {}
		ControlState GetState() const;
	private:
		const double& m_axis;
		const u8 m_index;
	};

	class Position : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		bool IsDetectable() { return false; }
		Position(u8 index, const double& axis) : m_index(index), m_axis(axis) {}
		ControlState GetState() const;
	private:
		const double& m_axis;
		const u8 m_index;
	};

	static void TrackerCallback(void* userdata, const OSVR_TimeValue* timestamp, const OSVR_PoseReport* report);
public:
	void UpdateInput() override;

	Tracker(const OSVR_ClientInterface& device, const char* path, u8 index);

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	OSVR_TimeValue m_last_update;
	OSVR_PoseReport m_pose_report;

	const OSVR_ClientInterface m_device;
	const char* m_path;
	const u8 m_index;
};

class Controller : public Core::Device
{
private:
	class Button : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Button(u8 index, const u8& button) : m_index(index), m_button(button) {}
		ControlState GetState() const;
	private:
		const u8& m_button;
		const u8 m_index;
	};

	class Axis : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Axis(u8 index, const double& axis, bool negative) : m_index(index), m_axis(axis), m_negative(negative) {}
		ControlState GetState() const;
	private:
		const double& m_axis;
		const bool m_negative;
		const u8 m_index;
	};

	class Trigger : public Core::Device::Input
	{
	public:
		std::string GetName() const;
		Trigger(u8 index, const double& trigger) : m_index(index), m_trigger(trigger) {}
		ControlState GetState() const;
	private:
		const double& m_trigger;
		const u8 m_index;
	};

	static void ButtonCallback(void *userdata, const struct OSVR_TimeValue *timestamp, const struct OSVR_ButtonReport *report);
	static void AnalogCallback(void *userdata, const struct OSVR_TimeValue *timestamp, const struct OSVR_AnalogReport *report);
public:
	void UpdateInput() override;

	Controller(const OSVR_ClientInterface& device, const char* path, u8 index);

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;

private:
	OSVR_ButtonState m_buttons[OSVR_NUM_BUTTONS];
	OSVR_AnalogState m_axes[OSVR_NUM_AXES];
	OSVR_AnalogState m_triggers[OSVR_NUM_AXES];

	const OSVR_ClientInterface m_device;
	const char* m_path;
	const u8 m_index;
};


}
}
