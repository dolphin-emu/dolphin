// Copyright 2013 Dolphin Emulator Project
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
			Code() : address(0), data(0) {}

			u32 address, data;

			std::string original_line;

			u32 GetAddress() const;
		};

		std::vector<Code> codes;
		std::string name, creator;
		std::vector<std::string> notes;

		bool enabled;
		bool user_defined;

		bool Compare(GeckoCode compare) const;
		bool Exist(u32 address, u32 data) const;
	};

	void SetActiveCodes(const std::vector<GeckoCode>& gcodes);
	bool RunActiveCodes();
	void RunCodeHandler();

} // namespace Gecko
