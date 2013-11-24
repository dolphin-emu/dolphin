// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
#ifndef _CIFACE_ANDROID_H_
#define _CIFACE_ANDROID_H_

#include "../Device.h"
#include "Android/ButtonManager.h"

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
		Button(int padID, ButtonManager::ButtonType index) :  _padID(padID), _index(index) {}
		ControlState GetState() const;
	private:
		const int _padID;
		const ButtonManager::ButtonType _index;
	};
	class Axis : public Input
	{
	public:
		std::string GetName() const;
		Axis(int padID, ButtonManager::ButtonType index) : _padID(padID), _index(index) {}
		ControlState GetState() const;
	private:
		const int _padID;
		const ButtonManager::ButtonType _index;
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
	const int _padID;
};

}
}

#endif
