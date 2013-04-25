// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
	void HBReload();
	void OSBootDol();
	void OSGetResetCode();
	void memcpy();
	void memset();
	void memmove();
	void memcmp();
	void div2i();
	void div2u();
	void ExecuteDOL(u8* dolFile, u32 fileSize);
}

#endif
