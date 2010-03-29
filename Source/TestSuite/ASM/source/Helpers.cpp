
#include "Defines.h"

u32 GetCR0()
{
	u32 var;
	asm(
		"mfcr %0"
		: "=&r"(var)
		);
	return var;
}

u32 GetCR(u32 num)
{
	u32 var;
	if(num == 0) // wtf, silly people
		return GetCR0();
	else {
		// TODO: Ugly switch is ugly, was failing otherwise
		switch (num) {
			case 1:
				asm("mcrf 0, 1");
				break;
			case 2:
				asm("mcrf 0, 2");
				break;
			case 3:
				asm("mcrf 0, 3");
				break;
			case 4:
				asm("mcrf 0, 4");
				break;
			case 5:
				asm("mcrf 0, 5");
				break;
			case 6:
				asm("mcrf 0, 6");
				break;
			case 7:
				asm("mcrf 0, 7");
				break;
			default:
				printf("Can this be more than 7?\n");
				break;
		}
		return GetCR0();
	}
}

u32 GetXER()
{
	u32 var;
	asm(
		"mfxer %0"
		: "=&r"(var)
		);
	return var;
}