// Copyright 2007,2008  Segher Boessenkool  <segher@kernel.crashing.org>
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#pragma once

#include "Common/CommonTypes.h"

void generate_ecdsa(u8* R, u8* S, const u8* k, const u8* hash);

void ec_priv_to_pub(const u8* k, u8* Q);

void point_mul(u8* d, const u8* a, const u8* b);
