// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#pragma once

#include <array>

#include "Common/CommonTypes.h"

namespace Common::ec
{
/// Generate a signature using ECDSA.
std::array<u8, 60> Sign(const u8* key, const u8* hash);

/// Compute a shared secret from a private key (30 bytes) and public key (60 bytes).
std::array<u8, 60> ComputeSharedSecret(const u8* private_key, const u8* public_key);

/// Convert a ECC private key (30 bytes) to a public key (60 bytes).
std::array<u8, 60> PrivToPub(const u8* key);
}  // namespace Common::ec
