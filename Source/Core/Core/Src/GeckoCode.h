// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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

		bool Compare(GeckoCode compare) const;
		bool Exist(u32 address, u32 data);
	};

	void SetActiveCodes(const std::vector<GeckoCode>& gcodes);
	bool RunActiveCodes();
	void RunCodeHandler();
	const std::map<u32, std::vector<u32> >& GetInsertedAsmCodes();

}	// namespace Gecko

#endif
