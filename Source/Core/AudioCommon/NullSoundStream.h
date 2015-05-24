// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdlib>
#include "AudioCommon/SoundStream.h"

#define BUF_SIZE (48000 * 4 / 32)

class NullSound final : public SoundStream
{
	// playback position
	short realtimeBuffer[BUF_SIZE / sizeof(short)];

public:
	bool Start() override;
	void SoundLoop() override;
	void SetVolume(int volume) override;
	void Stop() override;
	void Clear(bool mute) override;
	void Update() override;

	static bool isValid() { return true; }
};
