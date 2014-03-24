// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "DolphinWX/Android/ButtonManager.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ciface
{
namespace Android
{

void Init( std::vector<Core::Device*>& devices );
class Touchscreen : public Core::Device
{
private:
	class Button : public Input
	{
	public:
		std::string GetName() const;
		Button(int _padID, ButtonManager::ButtonType _index) :  padID(_padID), index(_index) {}
		ControlState GetState() const;
	private:
		const int padID;
		const ButtonManager::ButtonType index;
	};
	class Axis : public Input
	{
	public:
		std::string GetName() const;
		Axis(int _padID, ButtonManager::ButtonType _index, float _neg = 1.0f) : padID(_padID), index(_index), neg(_neg) {}
		ControlState GetState() const;
	private:
		const int padID;
		const ButtonManager::ButtonType index;
		const float neg;
	};
	
public:
	bool UpdateInput() { return true; }
	bool UpdateOutput() { return true; }

	Touchscreen(int padID);
	~Touchscreen() {}

	std::string GetName() const;
	int GetId() const;
	std::string GetSource() const;
private:
	const int padID;
};

}
}
