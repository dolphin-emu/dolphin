// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "Common/CommonTypes.h"

constexpr std::array<u64, 57> double_test_values{
    // Special values
    0x0000'0000'0000'0000,  // positive zero
    0x0000'0000'0000'0001,  // smallest positive denormal
    0x0000'0000'0100'0000,
    0x000F'FFFF'FFFF'FFFF,  // largest positive denormal
    0x0010'0000'0000'0000,  // smallest positive normal
    0x0010'0000'0000'0002,
    0x3FF0'0000'0000'0000,  // 1.0
    0x7FEF'FFFF'FFFF'FFFF,  // largest positive normal
    0x7FF0'0000'0000'0000,  // positive infinity
    0x7FF0'0000'0000'0001,  // first positive SNaN
    0x7FF7'FFFF'FFFF'FFFF,  // last positive SNaN
    0x7FF8'0000'0000'0000,  // first positive QNaN
    0x7FFF'FFFF'FFFF'FFFF,  // last positive QNaN
    0x8000'0000'0000'0000,  // negative zero
    0x8000'0000'0000'0001,  // smallest negative denormal
    0x8000'0000'0100'0000,
    0x800F'FFFF'FFFF'FFFF,  // largest negative denormal
    0x8010'0000'0000'0000,  // smallest negative normal
    0x8010'0000'0000'0002,
    0xBFF0'0000'0000'0000,  // -1.0
    0xFFEF'FFFF'FFFF'FFFF,  // largest negative normal
    0xFFF0'0000'0000'0000,  // negative infinity
    0xFFF0'0000'0000'0001,  // first negative SNaN
    0xFFF7'FFFF'FFFF'FFFF,  // last negative SNaN
    0xFFF8'0000'0000'0000,  // first negative QNaN
    0xFFFF'FFFF'FFFF'FFFF,  // last negative QNaN

    // (exp > 896) Boundary case for converting to single
    0x3800'0000'0000'0000,  // 2^(-127) = Denormal in single-prec
    0x3810'0000'0000'0000,  // 2^(-126) = Smallest single-prec normal
    0xB800'0000'0000'0000,  // -2^(-127) = Denormal in single-prec
    0xB810'0000'0000'0000,  // -2^(-126) = Smallest single-prec normal
    0x3800'1234'5678'9ABC, 0x3810'1234'5678'9ABC, 0xB800'1234'5678'9ABC, 0xB810'1234'5678'9ABC,

    // (exp >= 874) Boundary case for converting to single
    0x3680'0000'0000'0000,  // 2^(-150) = Unrepresentable in single-prec
    0x36A0'0000'0000'0000,  // 2^(-149) = Smallest single-prec denormal
    0x36B0'0000'0000'0000,  // 2^(-148) = Single-prec denormal
    0xB680'0000'0000'0000,  // -2^(-150) = Unrepresentable in single-prec
    0xB6A0'0000'0000'0000,  // -2^(-149) = Smallest single-prec denormal
    0xB6B0'0000'0000'0000,  // -2^(-148) = Single-prec denormal
    0x3680'1234'5678'9ABC, 0x36A0'1234'5678'9ABC, 0x36B0'1234'5678'9ABC, 0xB680'1234'5678'9ABC,
    0xB6A0'1234'5678'9ABC, 0xB6B0'1234'5678'9ABC,

    // (exp > 1148) Boundary case for fres
    0x47C0'0000'0000'0000,  // 2^125 = fres result is non-zero
    0x47D0'0000'0000'0000,  // 2^126 = fres result is zero
    0xC7C0'0000'0000'0000,  // -2^125 = fres result is non-zero
    0xC7D0'0000'0000'0000,  // -2^126 = fres result is zero

    // (exp < 895) Boundary case for fres
    0x37F0'0000'0000'0000,  // 2^(-128) = fres result is non-max
    0x37E0'0000'0000'0000,  // 2^(-129) = fres result is max
    0xB7F0'0000'0000'0000,  // -2^(-128) = fres result is non-max
    0xB7E0'0000'0000'0000,  // -2^(-129) = fres result is max

    // Some typical numbers
    0x3FF8'0000'0000'0000,  // 1.5
    0x408F'4000'0000'0000,  // 1000
    0xC008'0000'0000'0000,  // -3
};

constexpr std::array<u32, 33> single_test_values{
    // Special values
    0x0000'0000,  // positive zero
    0x0000'0001,  // smallest positive denormal
    0x0000'1000,
    0x007F'FFFF,  // largest positive denormal
    0x0080'0000,  // smallest positive normal
    0x0080'0002,
    0x3F80'0000,  // 1.0
    0x7F7F'FFFF,  // largest positive normal
    0x7F80'0000,  // positive infinity
    0x7F80'0001,  // first positive SNaN
    0x7FBF'FFFF,  // last positive SNaN
    0x7FC0'0000,  // first positive QNaN
    0x7FFF'FFFF,  // last positive QNaN
    0x8000'0000,  // negative zero
    0x8000'0001,  // smallest negative denormal
    0x8000'1000,
    0x807F'FFFF,  // largest negative denormal
    0x8080'0000,  // smallest negative normal
    0x8080'0002,
    0xBFF0'0000,  // -1.0
    0xFF7F'FFFF,  // largest negative normal
    0xFF80'0000,  // negative infinity
    0xFF80'0001,  // first negative SNaN
    0xFFBF'FFFF,  // last negative SNaN
    0xFFC0'0000,  // first negative QNaN
    0xFFFF'FFFF,  // last negative QNaN

    // (exp > 252) Boundary case for fres
    0x7E00'0000,  // 2^125 = fres result is non-zero
    0x7E80'0000,  // 2^126 = fres result is zero
    0xC7C0'0000,  // -2^125 = fres result is non-zero
    0xC7D0'0000,  // -2^126 = fres result is zero

    // Some typical numbers
    0x3FC0'0000,  // 1.5
    0x447A'0000,  // 1000
    0xC040'0000,  // -3
};
