// Copyright (C) 2003-2009 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef HLE_MISC_H
#define HLE_MISC_H

namespace HLE_Misc
{
    void Pass();
    void HLEPanicAlert();
    void UnimplementedFunction();
    void UnimplementedFunctionTrue();
    void UnimplementedFunctionFalse();
    void GXPeekZ();
    void GXPeekARGB();
    void SMB_EvilVecCosine();
    void SMB_EvilNormalize();
    void SMB_sqrt_internal();
    void SMB_rsqrt_internal();
    void SMB_atan2();
    void SMB_evil_vec_setlength();
	void FZero_kill_infinites();
	void FZero_evil_vec_normalize();
	void FZ_sqrt();
	void FZ_sqrt_internal();
	void FZ_rsqrt_internal();
}

#endif
