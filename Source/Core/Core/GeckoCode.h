// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Gecko
{

	class GeckoCode
	{
	public:

		GeckoCode() : enabled(false) {}

		struct Code
		{
			u32 address = 0;
			u32 data = 0;
			std::string original_line;
		};

		std::vector<Code> codes;
		std::string name, creator;
		std::vector<std::string> notes;

		bool enabled;
		bool user_defined;

		bool Compare(const GeckoCode& compare) const;
		bool Exist(u32 address, u32 data) const;
	};

	void SetActiveCodes(const std::vector<GeckoCode>& gcodes);
	bool RunActiveCodes();
	void RunCodeHandler();

} // namespace Gecko
