// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include <libevdev/libevdev.h>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace evdev
{

void Init(std::vector<Core::Device*>& devices);

class evdevDevice : public Core::Device
{
private:
	class Button : public Core::Device::Input
	{
	public:
		std::string GetName() const override;
		Button(u8 index, u16 code, libevdev* dev) : m_index(index), m_code(code), m_dev(dev) {}
		ControlState GetState() const override;
	private:
		const u8 m_index;
		const u16 m_code;
		libevdev* m_dev;
	};

	class Axis : public Core::Device::Input
	{
	public:
		std::string GetName() const override;
		Axis(u8 index, u16 code, bool upper, libevdev* dev);
		ControlState GetState() const override;
	private:
		const u16 m_code;
		const u8 m_index;
		const bool m_upper;
		int m_range;
		int m_min;
		libevdev* m_dev;
	};

	class ForceFeedback : public Core::Device::Output
	{
	public:
		std::string GetName() const override;
		ForceFeedback(u16 type, libevdev* dev) : m_type(type), m_id(-1) { m_fd = libevdev_get_fd(dev); }
		~ForceFeedback();
		void SetState(ControlState state) override;
	private:
		const u16 m_type;
		int m_fd;
		int m_id;
	};

public:
	void UpdateInput() override;

	evdevDevice(const std::string &devnode, int id);
	~evdevDevice();

	std::string GetName() const override { return m_name; }
	int GetId() const override { return m_id; }
	std::string GetSource() const override { return "evdev"; }

	bool IsInteresting() const { return m_initialized && m_interesting; }

private:
	const std::string m_devfile;
	int m_fd;
	libevdev* m_dev;
	std::string m_name;
	const int m_id;
	bool m_initialized;
	bool m_interesting;
};

}

}
