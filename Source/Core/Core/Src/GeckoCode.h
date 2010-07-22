
#ifndef __GECKOCODE_h__
#define __GECKOCODE_h__

#include "Common.h"

#include <vector>

namespace Gecko
{

	class GeckoCode
	{
	public:

		GeckoCode() : enabled(false) {}

		struct Code
		{
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

			u32 GetAddress() const;
		};

		std::vector<Code>	codes;
		std::string		name, description, creator;

		bool	enabled;
	};

	void SetActiveCodes(const std::vector<GeckoCode>& gcodes);
	bool RunActiveCodes();

}	// namespace Gecko

#endif