// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <osvr/ClientKit/ClientKitC.h>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace OSVR
{

void Init(std::vector<Core::Device*>& devices);
void DeInit();

class Device : public Core::Device
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

	Device(const OSVR_ClientInterface& device, const char* path, u8 index);

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


}
}
