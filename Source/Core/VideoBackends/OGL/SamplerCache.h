// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "VideoBackends/OGL/GLUtil.h"
#include "VideoBackends/OGL/Render.h"

namespace OGL
{

class SamplerCache : NonCopyable
{
public:
	SamplerCache();
	~SamplerCache();

	void SetSamplerState(int stage, const TexMode0& tm0, const TexMode1& tm1, bool custom_tex);
	void Clear();

private:
	struct Params
	{
		union
		{
			struct
			{
				TexMode0 tm0;
				TexMode1 tm1;
			};

			u64 hex;
		};

		Params()
			: hex()
		{}

		Params(const TexMode0& _tm0, const TexMode1& _tm1)
			: tm0(_tm0)
			, tm1(_tm1)
		{
			static_assert(sizeof(Params) == 8, "Assuming I can treat this as a 64bit int.");
		}

		bool operator<(const Params& other) const
		{
			return hex < other.hex;
		}

		bool operator!=(const Params& other) const
		{
			return hex != other.hex;
		}
	};

	struct Value
	{
		Value()
			: sampler_id()
		{}

		GLuint sampler_id;
	};

	void SetParameters(GLuint sampler_id, const Params& params);
	Value& GetEntry(const Params& params);

	std::map<Params, Value> m_cache;
	std::pair<Params, Value> m_active_samplers[8];

	int m_last_max_anisotropy;
};

extern SamplerCache *g_sampler_cache;

}
