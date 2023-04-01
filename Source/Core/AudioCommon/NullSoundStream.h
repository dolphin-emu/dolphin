// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "AudioCommon/SoundStream.h"

class NullSound final : public SoundStream
{
public:
	bool Start() override;
	void SoundLoop() override;
	void SetVolume(int volume) override;
	void Stop() override;
	void Clear(bool mute) override;
	void Update() override;

	static bool isValid() { return true; }

private:
	static constexpr size_t BUFFER_SIZE = 48000 * 4 / 32;

	// Playback position
	std::array<short, BUFFER_SIZE / sizeof(short)> m_realtime_buffer;
};
