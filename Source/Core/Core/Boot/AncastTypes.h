// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "Common/Crypto/SHA1.h"

// Magic number
constexpr u32 ANCAST_MAGIC = 0xEFA282D9;

// The location in memory where PPC ancast images are booted from
constexpr u32 ESPRESSO_ANCAST_LOCATION_PHYS = 0x01330000;
constexpr u32 ESPRESSO_ANCAST_LOCATION_VIRT = 0x81330000;

// The image type
enum AncastImageType
{
  ANCAST_IMAGE_TYPE_ESPRESSO_WIIU = 0x11,
  ANCAST_IMAGE_TYPE_ESPRESSO_WII = 0x13,
  ANCAST_IMAGE_TYPE_STARBUCK_NAND = 0x21,
  ANCAST_IMAGE_TYPE_STARBUCK_SD = 0x22,
};

// The console type of the image
enum AncastConsoleType
{
  ANCAST_CONSOLE_TYPE_DEV = 0x01,
  ANCAST_CONSOLE_TYPE_RETAIL = 0x02,
};

// Start of each header
struct AncastHeaderBlock
{
  u32 magic;
  u32 unknown;
  u32 header_block_size;
  u8 padding[0x14];
};
static_assert(sizeof(AncastHeaderBlock) == 0x20);

// Signature block for type 1
struct AncastSignatureBlockv1
{
  u32 signature_type;
  u8 signature[0x38];
  u8 padding[0x44];
};

// Signature block for type 2
struct AncastSignatureBlockv2
{
  u32 signature_type;
  u8 signature[0x100];
  u8 padding[0x7c];
};

// General info about the image
struct AncastInfoBlock
{
  u32 unknown;
  u32 image_type;
  u32 console_type;
  u32 body_size;
  Common::SHA1::Digest body_hash;
  u32 version;
  u8 padding[0x38];
};

// The header of espresso ancast images
struct EspressoAncastHeader
{
  AncastHeaderBlock header_block;
  AncastSignatureBlockv1 signature_block;
  AncastInfoBlock info_block;
};
static_assert(sizeof(EspressoAncastHeader) == 0x100);

// The header of starbuck ancast images
struct StarbuckAncastHeader
{
  AncastHeaderBlock header_block;
  AncastSignatureBlockv2 signature_block;
  AncastInfoBlock info_block;
};
static_assert(sizeof(StarbuckAncastHeader) == 0x200);
