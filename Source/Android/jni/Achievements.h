// Copyright 2003 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace Achievements
{
	enum class AchievementTypes
	{
		ACHIEVEMENT_CLOSETSPACE,
	};

	void Init();
	void Shutdown();

	void UnlockAchievement(AchievementTypes type);
	void IncrementAchievement(AchievementTypes type, int amount);
}
