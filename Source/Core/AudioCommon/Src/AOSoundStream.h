// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _AOSOUNDSTREAM_H_
#define _AOSOUNDSTREAM_H_

#include "SoundStream.h"

#if defined(HAVE_AO) && HAVE_AO
#include <ao/ao.h>
#endif

class AOSoundStream: public CBaseSoundStream
{
#if defined(HAVE_AO) && HAVE_AO
public:
	AOSoundStream(CMixer *mixer);
	virtual ~AOSoundStream();

	static inline bool IsValid() { return true; }

private:
	virtual void OnUpdate() override;

	virtual bool OnPreThreadStart() override;
	virtual void SoundLoop() override;
	virtual void OnPreThreadJoin() override;
	virtual void OnPostThreadJoin() override;

private:
	std::mutex m_soundCriticalSection;
	Common::Event m_soundSyncEvent;

	int m_buf_size;
	volatile bool m_join;

	ao_device *m_device;
	ao_sample_format m_format;
	int m_default_driver;

	short m_realtimeBuffer[1024 * 1024];

#else
public:
	AOSoundStream(CMixer *mixer):
		CBaseSoundStream(mixer)
	{
	}
#endif // defined(HAVE_AO) && HAVE_AO
};

#endif //_AOSOUNDSTREAM_H_
