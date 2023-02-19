// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// 13 JAN 2023 - Lilly Jade Katrin - lilly.kitty.1988@gmail.com
// Thanks to Stenzek and the PCSX2 project for inspiration, assistance and examples,
// to TheFetishMachine and Infernum for encouragement and cheerleading,
// and to Gollawiz, Sea, Fridge, jenette and Ryudo for testing

#pragma once
#include "Core/HW/Memmap.h"

#include <rcheevos/include/rc_api_info.h>
#include <rcheevos/include/rc_api_runtime.h>
#include <rcheevos/include/rc_api_user.h>
#include "rcheevos/include/rc_runtime.h"

struct rc_api_achievement_definition_t;

namespace Achievements
{
const std::string GRAY = "transparent";
const std::string GOLD = "#FFD700";
const std::string BLUE = "#0B71C1";

void Init();
void Login();
std::string Login(std::string password);
void StartSession(Memory::MemoryManager* memmgr);
void FetchData();
void ActivateAM();
void ActivateLB();
void ActivateRP();

void ResetGame();
void DoFrame();
void DoState(PointerWrap& p);
void Award(unsigned int achievement_id);
void Submit(unsigned int leaderboard_id, int value);
void Ping();

rc_api_login_response_t* GetUserStatus();
const std::vector<u8>* GetUserIcon();
rc_api_fetch_user_unlocks_response_t* GetHardcoreGameProgress();
rc_api_fetch_user_unlocks_response_t* GetSoftcoreGameProgress();
const std::vector<u8>* GetGameIcon();
rc_api_fetch_game_data_response_t* GetGameData();
int GetHardcoreAchievementStatus(unsigned int id);
int GetSoftcoreAchievementStatus(unsigned int id);
const std::vector<u8>* GetAchievementBadge(unsigned int id, bool locked);
void GetAchievementProgress(unsigned int id, unsigned* value, unsigned* target);
void GetLeader(unsigned int id, rc_api_fetch_leaderboard_info_response_t* response);
void GetCompetition(unsigned int id, rc_api_fetch_leaderboard_info_response_t* response);
std::string GetRichPresence();

void DeactivateAM();
void DeactivateLB();
void DeactivateRP();
void ResetSession();
void EndSession();
void Logout();
void Shutdown();
}  // namespace Achievements

// #ifdef ENABLE_RAINTEGRATION
namespace Achievements::RAIntegration
{
void ReinstallMemoryBanks();
void MainWindowChanged(void* new_handle);
void GameChanged(bool isWii);
void RAIDoFrame();
std::vector<std::tuple<int, std::string, bool>> GetMenuItems();
void ActivateMenuItem(int item);
}  // namespace Achievements::RAIntegration
// #endif
