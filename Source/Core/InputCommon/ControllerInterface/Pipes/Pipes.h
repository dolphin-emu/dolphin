// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <string>
#include <vector>

#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace Pipes
{
// To create a piped controller input, create a named pipe in the
// Pipes directory and write commands out to it. Commands are separated
// by a newline character, with spaces separating command tokens.
// Command syntax is as follows, where curly brackets are one-of and square
// brackets are inclusive numeric ranges. Cases are sensitive. Numeric inputs
// are clamped to [0, 1] and otherwise invalid commands are discarded.
// {PRESS, RELEASE} {A, B, X, Y, Z, START, L, R, D_UP, D_DOWN, D_LEFT, D_RIGHT}
// SET {L, R} [0, 1]
// SET {MAIN, C} [0, 1] [0, 1]

void Init(std::vector<Core::Device*>& devices);

class PipeDevice : public Core::Device
{
public:
	PipeDevice(int fd, const std::string& name, int id);
	~PipeDevice();

	void UpdateInput() override;
	std::string GetName() const override { return m_name; }
	int GetId() const override { return m_id; }
	std::string GetSource() const override { return "Pipe"; }

private:
	class PipeInput : public Input
	{
	public:
		PipeInput(const std::string& name) : m_name(name), m_state(0.0) {}
		std::string GetName() const override { return m_name; }
		ControlState GetState() const override { return m_state; }
		void SetState(ControlState state) { m_state = state; }
	private:
		const std::string m_name;
		ControlState m_state;
	};

	void AddAxis(const std::string& name, double value);
	void ParseCommand(const std::string& command);
	void SetAxis(const std::string& entry, double value);

	const int m_fd;
	const std::string m_name;
	const int m_id;
	std::string m_buf;
	std::map<std::string, PipeInput*> m_buttons;
	std::map<std::string, PipeInput*> m_axes;
};
}
}
