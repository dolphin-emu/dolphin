// Copyright 2021 Dolphin MMJR
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <Common/CommonTypes.h>

// Extract single color channel values from hex color code
#define RED_MASK(x) ((x >> 16u) & 0xFFu)
#define GREEN_MASK(x) ((x >> 8u) & 0xFFu)
#define BLUE_MASK(x) (x & 0xFFu)

namespace AndroidTheme
{
namespace
{
constexpr char MMJR_PURPLE[] = "mmjrpurple";
constexpr char NEON_RED[] = "neonred";
constexpr char DOLPHIN_BLUE[] = "dolphinblue";
constexpr char LUIGI_GREEN[] = "luigigreen";

constexpr u32 PURPLE = 0xFFFF00FF;
constexpr u32 RED = 0xFFFF0000;
constexpr u32 BLUE = 0xFF00FFFF;
constexpr u32 GREEN = 0xFF00FF00;

constexpr float F_PURPLE[3] = {RED_MASK(PURPLE) / 255.0f, GREEN_MASK(PURPLE) / 255.0f, BLUE_MASK(PURPLE) / 255.0f};
constexpr float F_RED[3] = {RED_MASK(RED) / 255.0f, GREEN_MASK(RED) / 255.0f, BLUE_MASK(RED) / 255.0f};
constexpr float F_BLUE[3] = {RED_MASK(BLUE) / 255.0f, GREEN_MASK(BLUE) / 255.0f, BLUE_MASK(BLUE) / 255.0f};
constexpr float F_GREEN[3] = {RED_MASK(GREEN) / 255.0f, GREEN_MASK(GREEN) / 255.0f, BLUE_MASK(GREEN) / 255.0f};

const float *currentFloat{F_PURPLE};
const u32 *currentInt{&PURPLE};
}  // Anonymous namespace

void Set(const std::string& theme);
const float *GetFloat();
const u32 *GetInt();
}  // namespace AndroidTheme
