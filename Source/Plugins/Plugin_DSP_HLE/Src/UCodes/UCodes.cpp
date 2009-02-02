// Copyright (C) 2003-2008 Dolphin Project.

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

#include "../Globals.h"

#include "UCodes.h"

#include "UCode_AX.h"
#include "UCode_AXWii.h"
#include "UCode_Zelda.h"
#include "UCode_Jac.h"
#include "UCode_ROM.h"
#include "UCode_CARD.h"
#include "UCode_InitAudioSystem.h"

IUCode* UCodeFactory(u32 _CRC, CMailHandler& _rMailHandler)
{
	switch (_CRC)
	{
	case UCODE_ROM:
		return new CUCode_Rom(_rMailHandler);
      
	case UCODE_INIT_AUDIO_SYSTEM:
		return new CUCode_InitAudioSystem(_rMailHandler);

	case 0x65d6cc6f: // CARD
		return new CUCode_CARD(_rMailHandler);

	case 0x088e38a5: // IPL - JAP
	case 0xd73338cf: // IPL
	case 0x42f64ac4: // Luigi       (after fix)
	case 0x4be6a5cb: // AC, Pikmin  (after fix)
		printf("JAC ucode chosen");
	return new CUCode_Jac(_rMailHandler);

	case 0x3ad3b7ac: // Naruto3
	case 0x3daf59b9: // Alien Hominid
	case 0x4e8a8b21: // spdemo, ctaxi, 18 wheeler, disney, monkeyball2,cubivore,puzzlecollection,wario,
	// capcom vs snk, naruto2, lost kingdoms, star fox, mario party 4, mortal kombat,
       // smugglers run warzone, smash brothers, sonic mega collection, ZooCube
       // nddemo, starfox
	case 0x07f88145: // bustamove, ikaruga, fzero, robotech battle cry, star soldier, soul calibur2,
       // Zelda:OOT, Tony hawk, viewtiful joe
	case 0xe2136399: // billy hatcher, dragonballz, mario party 5, TMNT, ava1080
		printf("AX ucode chosen, yay!");
		return new CUCode_AX(_rMailHandler);

	case 0x6CA33A6D: // DK Jungle Beat
	case 0x86840740: // zelda
	case 0x56d36052: // mario
	case 0x2fcdf1ec: // mariokart, zelda 4 swords
		printf("Zelda ucode chosen");
		return new CUCode_Zelda(_rMailHandler);

      // WII CRCs
	case 0x6c3f6f94: // zelda - PAL
	case 0xd643001f: // mario galaxy - PAL
		printf("Zelda Wii ucode chosen");
		return new CUCode_Zelda(_rMailHandler);

	case 0x5ef56da3: // AX demo
	case 0x347112ba: // raving rabbits
	case 0xfa450138: // wii sports - PAL
	case 0xadbc06bd: // Elebits
	case 0xb7eb9a9c: // Wii Pikmin - JAP
		printf("Wii - AXWii chosen");
		return new CUCode_AXWii(_rMailHandler, _CRC);

	default:
		PanicAlert("Unknown ucode (CRC = %08x) - forcing AX/AXWii", _CRC);
		if(g_dspInitialize.bWii)
			return new CUCode_AXWii(_rMailHandler, _CRC);
		return new CUCode_AX(_rMailHandler);
	}

	return NULL;
}


