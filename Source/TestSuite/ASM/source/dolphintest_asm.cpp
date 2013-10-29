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

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "Defines.h"
#include "Init.h"
#include "asm_tables.h"
#include "Helpers.h"

FILE *f = NULL;

int main(int argc, char **argv)
{
	init_crap();
	initialise_fat();

	for (uint a = 0; a < sizeof(instructions) / sizeof(inst); ++a)
	{
		RunInstruction(a);
	}

	end();

	return 0;
}
