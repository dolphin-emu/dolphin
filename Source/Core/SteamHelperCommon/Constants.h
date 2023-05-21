// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: BSD-3-Clause

#pragma once

// Our application ID, assigned by Valve.
static const int STEAM_APP_ID = 1941680;

// Used to verify that the helper app is being launched by Dolphin.
static const char* STEAM_HELPER_ENV_VAR_NAME = "LAUNCHED_BY_DOLPHIN";
