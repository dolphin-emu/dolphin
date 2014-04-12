// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <polarssl/md5.h>
#include "Common/Common.h"

class MD5Sum
{
private:
	md5_context m_ctx;

	void Starts() { md5_starts(&m_ctx); }

public:
	MD5Sum() { Starts(); }

	void Update(const u8* input, size_t len) { md5_update(&m_ctx, input, len); }

	void Finish(u8* output) { md5_finish(&m_ctx, output); }
};
