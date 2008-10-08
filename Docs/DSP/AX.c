//
// Pseudo C
//

//
// DSP:
//

// Memory USAGE

0x0B31 to 0x0B33			some kind of JMP-Table to handle srcSelect ???
0x0B34 to 0x0B36			some kind of JMP-Table to handle coefSelect ???
0x0B11 to 0x0B1F			some kind of JMP-Table to handle mixerCtrl ???
0x0B80 to 0x0C40			CurrentPB

0x0E15								SrcSelectFunction      // perhaps CMD0 setups some kind of jmp table at this addresses
0x0E16								CoefFunction
0x0E14								MixCtrlFunction


void CMD2()
{
// 0x1BC
	int_addrPB = (*r00 << 16) | *(r00+1)
	DMA_From_Memory(0x0B80, _addrPB, 0xC0);   // read first PB to 0x0B80
	
// 0x1C7	
	*0x0E08 = 0x0000
	*0x0E09 = 0x0140
	*0x0E0A = 0x0280
	*0x0E0B = 0x0400
	*0x0E0C = 0x0540
	*0x0E0D = 0x0680
	*0x0E0E = 0x07C0
	*0x0E0F = 0x0900
	*0x0E10 = 0x0A40

// 0x1E4
	WaitForDMATransfer()
	
// 0x1E6		
	Addr = (*0x0BA7 << 16) | *0x0BA8			
	DMA_From_Memory(0x03C0, Addr, 0x80);	// Copy Update Data to 0x03C0  (AXPBUPDATE dataHi, dataLo)
	
// 0x1F4	
	R03 = (*0x0B84) + 0x0B31		// AXPB->srcSelect + 0x0B31 ??? some kind of flag handling ??? SRCSEL can be 0x0 to 0x2
	AC0.M = *R03
	*0x0E15 = *AC0.M
	
// 0x1FD	
	R03 = (*0x0B85) + 0x0B34		// AXPB->coefSelect + 0x0B34 ??? some kind of flag handling ??? COEF can be 0x0 to 0x2
	AC0.M = *R03
	*0x0E16 = *AC0.M

// 0x206
	R03 = (*0x0B86) + 0x0B11		// AXPB->mixerCtrl + 0x0B36 ??? some kind of flag handling ???	MIXCTRL can be 0x0 to 0xE 
	AC0.M = *R03
	*0x0E14 = *AC0.M
	
// 0x20F	
	if (*0x0B9B == 0)						// AXPBITD->flag  (on or off for this voice)
	{
		// jmp to 0x23a
		*0x0E42 = 0x0CE0
		*0x0E40 = 0x0CE0
		*0x0E41 = 0x0CE0
		*0x0E43 = 0x0CE0	
		
		WaitForDMATransfer()
	}
	else
	{
		// code at 0x216
		*0x0E40 = *0x0B9E + 0x0CC0		// AXPBITD->shiftL
		*0x0E41 = *0x0B9F + 0x0CC0		// AXPBITD->shiftR
		*0x0E42 = 0xCE0
		*0x0E43 = 0xCE0
		
		WaitForDMATransfer()
		
		// 0x22a
		Addr = (*0x0B9C << 16) | *0x0B9D	
		DMA_From_Memory(0x0CC0, Addr, 0x40);		// (AXPBITD->bufferHi << 16 | AXPBITD->bufferLo) -> 0xCC0
		WaitForDMATransfer()
	}

}

void CMD0()
{
	
}


void CMD2()
{

	0x0E07 = R00
	R00 = 0x0BA2		// AXPBUPDATE->updNum
	R01 = 0x03C0
	*0x0E04 = 0x05
	
	AC1 = 0
	AC0 = 0
	AX0.H = *R00++   // AXPBUPDATE->updNum[0]
	AC1.M = 0x0B80

// 0x256
	for (i=0; i<AX0.H; i++)
	{
		AC0.M = *R01++
		AC0.M = AC0.M + AC1.M
		AX1.L = *R01++
		R02 = AC0.M
		*R02 = AX1.L
	}
	
// 0x25c
	R03 = 0x0E05
	*R03++ = R01
	*R03++ = R02
	
// 0x260
	AC0.M = *0x0B87   // AXPB->state

	if (AC0.M == 1)
	{
		// JMP 0x267 (AX_PB_STATE_RUN)
		*0x0E1C = *0x0E42
		
		CALLR *0x0E15   // Load Sample (SrcSelectFunction)
		
		// 0x270
		AC0.M = *0xBB3 	// AXPBVE->currentDelta		(.15 volume at start of frame)
		AC1.M = *0xBB2	// AXPBVE->currentVolume
		
		//  0x278
		AX0.L = AC1.M
		AC1.M = AC1.M + AC0.M
		AC0.M = AC0.M << 1
		
		SET15 // ????
		
		AX1.H = AC0.M
		AC0.M = AX0.L
		
		AX0.L = 0x8000
		R00 = 0x0E44
		
// 0x27f
		// scale volume table
		.
		.
		.
/*		for (int i=0; i<32; i++)
		{
			*R00++ = AC0.M; 			
			prod = AX0.L * AX1.H
			
			*R00++ = AC1.M; 
			AC0 = AC0 + prod
			prod = AX0.L * AX1.H

			*R00++ = AC0.M; 
			AC1 = AC1 + prod
			prod = AX0.L * AX1.H					
		}*/
				
// 0x29f
		*0xBB2 = CurrentVolume
		
		
// 0x2a1
		// mutiply volume with sample
		.
		.
		.
		
		
// 0x2ea
	// Call mixer
	
// 0x02f0

		
				
	}
	else
	{
		// JMP 0x332
		.
		.
		.
		.
	}

}

// ===============================================================
void Func_0x065D()
{
	
	
}