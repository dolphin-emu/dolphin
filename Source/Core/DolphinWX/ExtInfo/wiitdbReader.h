#pragma once
#include "../ISOFile.h"

class wiitdbReader
{
public:
	wiitdbReader();
	~wiitdbReader();

	bool FillExtendedInfos(const char * pFilename, std::vector<GameListItem*> gameList);

};



