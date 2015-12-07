// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

class ControllerEmu;

class InputConfig
{
public:
	InputConfig(const std::string& ini_name, const std::string& gui_name, const std::string& profile_name)
		: m_ini_name(ini_name), m_gui_name(gui_name), m_profile_name(profile_name)
	{
	}

	bool LoadConfig(bool isGC);
	void SaveConfig();

	template <typename T, typename... Args>
	void CreateController(Args&&... args)
	{
		m_controllers.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
	}

	ControllerEmu* GetController(int index);
	void ClearControllers();
	bool ControllersNeedToBeCreated() const;

	std::string GetGUIName() const { return m_gui_name; }
	std::string GetProfileName() const { return m_profile_name; }

private:
	std::vector<std::unique_ptr<ControllerEmu>> m_controllers;
	const std::string m_ini_name;
	const std::string m_gui_name;
	const std::string m_profile_name;
};
