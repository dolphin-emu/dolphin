// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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
	virtual bool Start() override;
	virtual void SoundLoop() override;
	virtual void SetVolume(int volume) override;
	virtual void Stop() override;
	virtual void Clear(bool mute) override;
	static bool isValid() { return true; }
	virtual void Update() override;
};
