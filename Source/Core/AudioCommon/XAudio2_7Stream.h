// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This audio backend uses XAudio2 via XAudio2_7.dll
// This version of the library is included in the June 2010 DirectX SDK and
// works on all versions of Windows, however the SDK and/or redist must be
// separately installed.
// Therefore this backend is available iff:
//  * SDK is available at compile-time
//  * runtime dll is available at runtime
// Dolphin ships the relevant SDK headers in Externals, so it's always available.

#pragma once

#include <memory>
#include "AudioCommon/SoundStream.h"
#include "Common/Event.h"

#ifdef _WIN32

#include <Windows.h>

struct StreamingVoiceContext2_7;
struct IXAudio2;
struct IXAudio2MasteringVoice;

#endif

class XAudio2_7 final : public SoundStream
{
#ifdef _WIN32
protected:
	virtual void InitializeSoundLoop() override;
	virtual u32 SamplesNeeded() override;
	virtual void WriteSamples(s16 *src, u32 numsamples) override;
	virtual bool SupportSurroundOutput() override;
private:
	static void ReleaseIXAudio2(IXAudio2 *ptr);

	class Releaser
	{
	public:
		template <typename R>
		void operator()(R *ptr)
		{
			ReleaseIXAudio2(ptr);
		}
	};

	std::unique_ptr<IXAudio2, Releaser> m_xaudio2;
	std::unique_ptr<StreamingVoiceContext2_7> m_voice_context;
	IXAudio2MasteringVoice *m_mastering_voice;
	float m_volume;

	const bool m_cleanup_com;

	static HMODULE m_xaudio2_dll;

	static bool InitLibrary();
	u32 samplesize;

public:
	XAudio2_7();
	~XAudio2_7() override;

	bool Start() override;
	void Stop() override;
	void Clear(bool mute) override;
	void SetVolume(int volume) override;

	static bool isValid() { return InitLibrary(); }

#else

public:
	XAudio2_7()
	{}

#endif
};
