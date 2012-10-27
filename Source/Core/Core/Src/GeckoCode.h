// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

#ifndef __GECKOCODE_h__
#define __GECKOCODE_h__

#include "Common.h"

#include <vector>
#include <string>
#include <map>

namespace Gecko
{

	class GeckoCode
	{
	public:

		GeckoCode() : enabled(false) {}

		struct Code
		{
			Code() : address(0), data(0) {}

			union
			{
				u32	address;

				struct
				{
					u32 gcaddress : 25;
					u32 subtype: 3;
					u32 use_po : 1;
					u32 type: 3;
				};

				struct
				{
					u32 n : 4;
					u32 z : 12;
					u32 y : 4;
					u32 t : 4;
					//u32 s : 4;
					//u32 : 4;
				};// subsubtype;
			};

			union
			{
				u32 data;
				//struct
				//{
				//	
				//};
			};

			std::string original_line;

			u32 GetAddress() const;
		};

		std::vector<Code>	codes;
		std::string		name, creator;
		std::vector<std::string>	notes;

		bool	enabled;
	};

	void SetActiveCodes(const std::vector<GeckoCode>& gcodes);
	bool RunActiveCodes();
	void RunCodeHandler();
	const std::map<u32, std::vector<u32> >& GetInsertedAsmCodes();

}	// namespace Gecko

#endif
