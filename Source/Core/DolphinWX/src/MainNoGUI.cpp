#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
//#include <curses.h>
#else
#endif

#include "Globals.h"
#include "Common.h"
#include "ISOFile.h"

#include "BootManager.h"

int main(int argc, const char* argv[])
{
	if (argc != 2)
	{
		puts("Please supply at least one argument - the ISO to boot.\n");
		return(1);
	}

	CISOFile iso(argv[1]);

	if (!iso.IsValid())
	{
		printf("The ISO %s is not a valid Gamecube or Wii ISO.", argv[1]);
		return(1);
	}

	BootManager::BootCore(iso);
	usleep(2000 * 1000 * 1000);
//	while (!getch()) {
	//	usleep(20);
//	}
	return(0);
}


