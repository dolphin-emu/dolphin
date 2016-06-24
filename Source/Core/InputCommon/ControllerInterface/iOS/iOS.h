// Copyright 20016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "InputCommon/ControllerInterface/Device.h"
#include "InputCommon/ControllerInterface/iOS/iOSButtonManager.h"

namespace ciface
{
namespace iOS
{
	void Init(std::vector<Core::Device*>& devices);
	class Touchscreen : public Core::Device
	{
	private:
		class Button : public Input
		{
		public:
			std::string GetName() const;
			Button(int padID, ButtonManager::ButtonType index) : _padID(padID), _index(index) {}
			ControlState GetState() const;
		private:
			const int _padID;
			const ButtonManager::ButtonType _index;
		};
		class Axis : public Input
		{
		public:
			std::string GetName() const;
			Axis(int padID, ButtonManager::ButtonType index, float neg = 1.0f) : _padID(padID), _index(index), _neg(neg) {}
			ControlState GetState() const;
		private:
			const int _padID;
			const ButtonManager::ButtonType _index;
			const float _neg;
		};

	public:
		Touchscreen(int padID);
		~Touchscreen() {}

		std::string GetName() const;
		int GetId() const;
		std::string GetSource() const;
	private:
		const int _padID;
	};

} // namespace iOS
} // namespace ciface
