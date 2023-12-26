// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>

#include "Common/CommonTypes.h"

namespace IOS::HLE::USB::SkylanderCrypto
{
u16 ComputeCRC16(std::span<const u8> data);
u64 ComputeCRC48(std::span<const u8> data);
u64 CalculateKeyA(u8 sector, std::span<const u8, 0x4> nuid);
void ComputeChecksumType0(const u8* data_start, u8* output);
void ComputeChecksumType1(const u8* data_start, u8* output);
void ComputeChecksumType2(const u8* data_start, u8* output);
void ComputeChecksumType3(const u8* data_start, u8* output);
void ComputeChecksumType6(const u8* data_start, u8* output);
std::array<u8, 11> ComputeToyCode(u64 code);
}  // namespace IOS::HLE::USB::SkylanderCrypto
