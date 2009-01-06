//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////



// Includes

#include <iostream>
#include <vector>
#include <string> // so that we can test std::string == abc
#include <math.h> // for the pow() function
#ifdef _WIN32
	#include <windows.h>
#endif

#include "StringUtil.h"

#include "../Debugger/Debugger.h"
#include "../Debugger/PBView.h"
#include "Console.h" // open and close console, clear console window

#include "../Globals.h"
#include "../UCodes/UCodes.h"
#include "../UCodes/UCode_AXStructs.h"
#include "../UCodes/UCode_AX.h"
#include "../UCodes/UCode_AXWii.h"
#include "../UCodes/UCode_AX_Voice.h"

// Externals
extern int nFiles;
extern CDebugger* m_frame;
//int PBSize = 128;

// Parameter blocks

	std::vector<int> mem(NUMBER_OF_PBS); // mem1 or mem2
	std::vector<u32> gloopPos(NUMBER_OF_PBS);
	std::vector<u32> gsampleEnd(NUMBER_OF_PBS);
	std::vector<u32> gsamplePos(NUMBER_OF_PBS);

// main
	std::vector<u16> running(NUMBER_OF_PBS, 0);
	std::vector<u16> gsrc_type(NUMBER_OF_PBS);
	std::vector<u16> gis_stream(NUMBER_OF_PBS);

// PBSampleRateConverter src
	std::vector<u32> gratio(NUMBER_OF_PBS);
	std::vector<u32> gratiohi(NUMBER_OF_PBS);
	std::vector<u32> gratiolo(NUMBER_OF_PBS);
	std::vector<u32> gfrac(NUMBER_OF_PBS);
	std::vector<u32> gcoef(NUMBER_OF_PBS);

// PBSampleRateConverter mixer
	std::vector<u16> gvolume_left(NUMBER_OF_PBS);
	std::vector<u16> gmix_unknown(NUMBER_OF_PBS);
	std::vector<u16> gvolume_right(NUMBER_OF_PBS);
	std::vector<u16> gmix_unknown2(NUMBER_OF_PBS);
	std::vector<u16> gmixer_control(NUMBER_OF_PBS);
	std::vector<u32> gmixer_control_wii(NUMBER_OF_PBS);

	std::vector<u16> gmixer_vol1(NUMBER_OF_PBS);
	std::vector<u16> gmixer_vol2(NUMBER_OF_PBS);
	std::vector<u16> gmixer_vol3(NUMBER_OF_PBS);
	std::vector<u16> gmixer_vol4(NUMBER_OF_PBS);
	std::vector<u16> gmixer_vol5(NUMBER_OF_PBS);
	std::vector<u16> gmixer_vol6(NUMBER_OF_PBS);
	std::vector<u16> gmixer_vol7(NUMBER_OF_PBS);

	std::vector<u16> gmixer_d1(NUMBER_OF_PBS);
	std::vector<u16> gmixer_d2(NUMBER_OF_PBS);
	std::vector<u16> gmixer_d3(NUMBER_OF_PBS);
	std::vector<u16> gmixer_d4(NUMBER_OF_PBS);
	std::vector<u16> gmixer_d5(NUMBER_OF_PBS);
	std::vector<u16> gmixer_d6(NUMBER_OF_PBS);
	std::vector<u16> gmixer_d7(NUMBER_OF_PBS);

// PBVolumeEnvelope vol_env
	std::vector<u16> gcur_volume(NUMBER_OF_PBS);
	std::vector<u16> gcur_volume_delta(NUMBER_OF_PBS);

// PBAudioAddr audio_addr (incl looping)
	std::vector<u16> gaudioFormat(NUMBER_OF_PBS);
	std::vector<u16> glooping(NUMBER_OF_PBS);
	std::vector<u16> gloop1(NUMBER_OF_PBS);
	std::vector<u16> gloop2(NUMBER_OF_PBS);
	std::vector<u16> gloop3(NUMBER_OF_PBS);

// PBADPCMInfo adpcm
	std::vector<u16> gadloop1(NUMBER_OF_PBS);
	std::vector<u16> gadloop2(NUMBER_OF_PBS);
	std::vector<u16> gadloop3(NUMBER_OF_PBS);

// updates
	std::vector<u16> gupdates1(NUMBER_OF_PBS);
	std::vector<u16> gupdates2(NUMBER_OF_PBS);
	std::vector<u16> gupdates3(NUMBER_OF_PBS);
	std::vector<u16> gupdates4(NUMBER_OF_PBS);
	std::vector<u16> gupdates5(NUMBER_OF_PBS);
	std::vector<u32> gupdates_addr(NUMBER_OF_PBS);
	std::vector<u32> gupdates_data(NUMBER_OF_PBS);
	std::vector<u32> gupdates_data1(NUMBER_OF_PBS);
	std::vector<u32> gupdates_data2(NUMBER_OF_PBS);
	std::vector<u32> gupdates_data3(NUMBER_OF_PBS);
	std::vector<u32> gupdates_data4(NUMBER_OF_PBS);

// Counters

int count1 = 0;
int count2 = 0;
int iupd = 0;
bool iupdonce = false;
std::vector<u16> viupd(15); // the length of the update frequency bar
int vectorLengthGUI = 8; // length of playback history bar for the GUI version
int vectorLength = 15; // for console version
int vectorLength2 = 100; // for console version, how long to show

// More stuff

// should we worry about the additonal memory these lists require? bool will allocate
// very little memory
std::vector< std::vector<bool> > vector1(NUMBER_OF_PBS, std::vector<bool>(vectorLength, false)); 
std::vector< std::vector<bool> > vector2(NUMBER_OF_PBS, std::vector<bool>(vectorLength2, false));
std::vector<int> numberRunning(NUMBER_OF_PBS);


// Classes

extern CDebugger* m_frame;
		


// =======================================================================================
// Write title
// --------------
std::string writeTitle(int a, bool Wii)
{
	std::string b;
	if(a == 0)
	{		
		if(m_frame->bShowBase) // show base 10
		{
			b =     "                                                                                  adpcm           adpcm_loop\n";
			b = b + "                Nr m       pos / end        lpos      | voll  volr    | isl iss | pre yn1   yn2   pre yn1   yn2   | frac  ratio[hi lo]\n";
		}
		else
		{
			b =     "                                                                      adpcm         adpcm_loop\n";
			b = b + "                Nr     pos / end     lpos     | voll volr | isl iss | pre yn1  yn2  pre yn1  yn2  | frac rati[hi lo   ]\n";
		}
	}
	else if(a == 1)
	{
		if(m_frame->bShowBase) // show base 10
		{
			b = "                Nr       pos / end       lpos       | voll   volr   | src form coef | 1 2 3 4 5 addr     value\n";
		}
		else
		{
			b = "                Nr     pos / end     lpos     | voll volr | src form coef | 1 2 3 4 5 addr     value\n";
		}
	}
	else if(a == 2)
	{
		if(m_frame->bShowBase) // show base 10
		{
			b = "                Nr     pos / end      lpos          | voll   volr | isl iss | e-l      e-s\n";
		}
		else
		{
			b = "                Nr     pos / end     lpos     | voll volr | isl iss | e-l       e-s\n";
		}
	}
	else if(a == 3)
	{
		if(m_frame->bShowBase) // show base 10
		{
			if(Wii)
				b = "                Nr  voll  volr  dl    dr    curv  delt  mixc     r | v1    v2    v3    v4    v5    v6    v7    | d1    d2    d3    d4    d5    d6    d7\n";
			else
				b = "                Nr  voll  volr  dl    dr    curv  delt  mixc  r | v1    v2    v3    v4    v5    v6    v7    | d1    d2    d3    d4    d5    d6    d7\n";
		}
		else
		{
			if(Wii)
				b = "                Nr  voll volr dl   dr   curv delt mixc     r | v1   v2   v3   v4   v5   v6   v7   | d1   d2   d3   d4   d5   d6   d7\n";
			else
				b = "                Nr  voll volr dl   dr   curv delt mixc r | v1   v2   v3   v4   v5   v6   v7   | d1   d2   d3   d4   d5   d6   d7\n";
		}
	}	
	return b;
}
// =======================================================================================



// =======================================================================================
// Write main message (presets)
// --------------
std::string writeMessage(int a, int i, bool Wii)
{
	char buf [1000] = "";
	std::string sbuf;
	// =======================================================================================
	// PRESETS
	// ---------------------------------------------------------------------------------------
	/*
	PRESET 0
	"                Nr m      pos / end        lpos     | voll   volr   | isl iss | pre yn1   yn2   pre yn1   yn2   | frac  ratio[hi lo]\n";
	"---------------|00 1 12,341,234/134,123,412 12341234 | 00,000 00,000 | 0   0   | 000 00000 00000 000 00000 00000 | 00000 00000[0 00000] 
			
		"                Nr       pos / end        lpos     | voll   volr   | isl iss | pre yn1  yn2  pre yn1  yn2  | frac rati[hi lo   ]\n";
		"---------------|00 12,341,234/134,123,412 12341234 | 00,000 00,000 | 0   0   | 000 0000 0000 000 0000 0000 | 0000 0000[0  00000] 
	
	PRESET 1 (updates)
	"                Nr       pos / end       lpos     | voll  volr    | src form coef | 1 2 3 4 5 addr     value\n";
	"---------------|00 12,341,234/12,341,234 12341234 | 00,000 00,000 | 0   0    0    | 0 0 0 0 0 80808080 80808080

	PRESET 2
	"                Nr       pos / end     lpos       | voll  volr  | isl iss | e-l        e-s\n";
	"---------------|00 12,341,234/12341234 12,341,234 | 00000 00000 | 0   0   | 00,000,000 00,000,000
	*/
	if(a == 0)
	{
		if(m_frame->bShowBase) // base 10 (decimal)
		{
			if(Wii) // Wii
			{
			sprintf(buf,"%c%02i %i %10s/%10s %10s | %6s %6s | %i   %i   | %03i %05i %05i %03i %05i %05i | %05i %05i[%i %05i]",
				223, i, mem[i], ThS(gsamplePos[i],true).c_str(), ThS(gsampleEnd[i],true).c_str(), ThS(gloopPos[i],true).c_str(),
				ThS(gvolume_left[i]).c_str(), ThS(gvolume_right[i]).c_str(),
				glooping[i], gis_stream[i],
				gadloop1[i], gadloop2[i], gadloop3[i], gloop1[i], gloop2[i], gloop3[i],
				gfrac[i], gratio[i], gratiohi[i], gratiolo[i]
				);
			}
			else // GC
			{
			sprintf(buf,"%c%02i %10s/%10s %10s | %6s %6s | %i   %i   | %03i %05i %05i %03i %05i %05i | %05i %05i[%i %05i]",
				223, i, ThS(gsamplePos[i],true).c_str(), ThS(gsampleEnd[i],true).c_str(), ThS(gloopPos[i],true).c_str(),
				ThS(gvolume_left[i]).c_str(), ThS(gvolume_right[i]).c_str(),
				glooping[i], gis_stream[i],
				gadloop1[i], gadloop2[i], gadloop3[i], gloop1[i], gloop2[i], gloop3[i],
				gfrac[i], gratio[i], gratiohi[i], gratiolo[i]
				);
			}
		}
		else
		{
		sprintf(buf,"%c%02i %08x/%08x %08x | %04x %04x | %i   %i   | %02x  %04x %04x %02x  %04x %04x | %04x %04x[%i %04x]",
			223, i, gsamplePos[i], gsampleEnd[i], gloopPos[i],
			gvolume_left[i], gvolume_right[i],
			glooping[i], gis_stream[i],
			gadloop1[i], gadloop2[i], gadloop3[i], gloop1[i], gloop2[i], gloop3[i],
			gfrac[i], gratio[i], gratiohi[i], gratiolo[i]
			);
		}
	}
	else if(a == 1)
	{
		if(m_frame->bShowBase) // base 10 (decimal)
		{
                    sprintf(buf,"%c%02i %10s/%10s %10s | %6s %6s | %u   %u    %u    | %u %u %u %u %u %08x %08x %08x %08x %08x %08x",
			223, i, ThS(gsamplePos[i]).c_str(), ThS(gsampleEnd[i]).c_str(), ThS(gloopPos[i]).c_str(),
			ThS(gvolume_left[i]).c_str(), ThS(gvolume_right[i]).c_str(),		
			gsrc_type[i], gaudioFormat[i], gcoef[i],
			gupdates1[i], gupdates2[i], gupdates3[i], gupdates4[i], gupdates5[i], gupdates_addr[i],
			gupdates_data[i], gupdates_data1[i], gupdates_data2[i], gupdates_data3[i], gupdates_data4[i] 
			);
		}
		else // base 16 (hexadecimal)
		{
			if(Wii) // Wii
			{
			sprintf(buf,"%c%02i %08x/%08x %08x | %04x %04x | %u   %u    %u    | %u %u %u %08x %08x %08x %08x %08x %08x",
				223, i, gsamplePos[i], gsampleEnd[i], gloopPos[i],
				gvolume_left[i], gvolume_right[i],		
				gsrc_type[i], gaudioFormat[i], gcoef[i],
				gupdates1[i], gupdates2[i], gupdates3[i], gupdates_addr[i],
				gupdates_data[i], gupdates_data1[i], gupdates_data2[i], gupdates_data3[i], gupdates_data4[i] 
				);
			}
			else // GC
			{
			sprintf(buf,"%c%02i %08x/%08x %08x | %04x %04x | %u   %u    %u    | %u %u %u %u %u %08x %08x %08x %08x %08x %08x",
				223, i, gsamplePos[i], gsampleEnd[i], gloopPos[i],
				gvolume_left[i], gvolume_right[i],		
				gsrc_type[i], gaudioFormat[i], gcoef[i],
				gupdates1[i], gupdates2[i], gupdates3[i], gupdates4[i], gupdates5[i], gupdates_addr[i],
				gupdates_data[i], gupdates_data1[i], gupdates_data2[i], gupdates_data3[i], gupdates_data4[i] 
				);
			}
		}
	}
	else if(a == 2)
	{
		if(m_frame->bShowBase)
		{
		sprintf(buf,"%c%02i %10s/%10s %10s | %05i %05i | %i   %i   | %10s     %10s",
			223, i, ThS(gsamplePos[i]).c_str(), ThS(gsampleEnd[i]).c_str(), ThS(gloopPos[i]).c_str(),
			gvolume_left[i], gvolume_right[i],
			glooping[i], gis_stream[i],				
			ThS(gsampleEnd[i] - gloopPos[i], false).c_str(), ThS(gsampleEnd[i] - gsamplePos[i], false).c_str()
			);
		}
		else
		{
		sprintf(buf,"%c%02i %08x/%08x %08x | %04x %04x | %i   %i   | %08x   %08x",
			223, i, gsamplePos[i], gsampleEnd[i], gloopPos[i],
			gvolume_left[i], gvolume_right[i],
			glooping[i], gis_stream[i],				
			gsampleEnd[i] - gloopPos[i], gsampleEnd[i] - gsamplePos[i]
			);
		}
	}
	/*
	PRESET 3
	"                Nr  voll  volr  dl    dr    curv  delt  mixc  r | v1    v2    v3    v4    v5    v6    v7    | d1    d2    d3    d4    d5    d6    d7\n";
	"---------------|00  00000 00000 00000 00000 00000 00000 00000 0 | 00000 00000 00000 00000 00000 00000 00000 | 00000 00000 00000 00000 00000 00000 00000
	*/
	else if(a == 3)
	{
		if(m_frame->bShowBase)
		{
			if(Wii) // Wii
			{
			sprintf(buf,"%c%02i  %05i %05i %05i %05i %05i %05i %05i %i | %05i %05i %05i %05i %05i %05i %05i | %05i %05i %05i %05i %05i %05i %05i",
				223, i,
				gvolume_left[i], gvolume_right[i], gmix_unknown[i], gmix_unknown2[i], gcur_volume[i], gcur_volume_delta[i],
					gmixer_control_wii[i], (gmixer_control_wii[i] & MIXCONTROL_RAMPING),
				gmixer_vol1[i], gmixer_vol2[i], gmixer_vol3[i], gmixer_vol4[i], gmixer_vol5[i],
				gmixer_vol6[i], gmixer_vol7[i],
				gmixer_d1[i], gmixer_d2[i], gmixer_d3[i], gmixer_d4[i], gmixer_d5[i],
				gmixer_d6[i], gmixer_d7[i]
				);
			}
			else // GC
			{
			sprintf(buf,"%c%02i  %05i %05i %05i %05i %05i %05i %08i %i | %05i %05i %05i %05i %05i %05i %05i | %05i %05i %05i %05i %05i %05i %05i",
				223, i,
				gvolume_left[i], gvolume_right[i], gmix_unknown[i], gmix_unknown2[i], gcur_volume[i], gcur_volume_delta[i],
					gmixer_control[i], (gmixer_control[i] & MIXCONTROL_RAMPING),
				gmixer_vol1[i], gmixer_vol2[i], gmixer_vol3[i], gmixer_vol4[i], gmixer_vol5[i],
				gmixer_vol6[i], gmixer_vol7[i],
				gmixer_d1[i], gmixer_d2[i], gmixer_d3[i], gmixer_d4[i], gmixer_d5[i],
				gmixer_d6[i], gmixer_d7[i]
				);
			}
		}
		else
		{
			if(Wii)
			{
			sprintf(buf,"%c%02i  %04x %04x %04x %04x %04x %04x %08x %i | %04x %04x %04x %04x %04x %04x %04x | %04x %04x %04x %04x %04x %04x %04x",
				223, i,
				gvolume_left[i], gvolume_right[i], gmix_unknown[i], gmix_unknown2[i], gcur_volume[i], gcur_volume_delta[i],
					gmixer_control_wii[i], (gmixer_control_wii[i] & MIXCONTROL_RAMPING),
				gmixer_vol1[i], gmixer_vol2[i], gmixer_vol3[i], gmixer_vol4[i], gmixer_vol5[i],
				gmixer_vol6[i], gmixer_vol7[i],
				gmixer_d1[i], gmixer_d2[i], gmixer_d3[i], gmixer_d4[i], gmixer_d5[i],
				gmixer_d6[i], gmixer_d7[i]
				);
			}
			else
			{
			sprintf(buf,"%c%02i  %04x %04x %04x %04x %04x %04x %04x %i | %04x %04x %04x %04x %04x %04x %04x | %04x %04x %04x %04x %04x %04x %04x",
				223, i,
				gvolume_left[i], gvolume_right[i], gmix_unknown[i], gmix_unknown2[i], gcur_volume[i], gcur_volume_delta[i],
					gmixer_control[i], (gmixer_control[i] & MIXCONTROL_RAMPING),
				gmixer_vol1[i], gmixer_vol2[i], gmixer_vol3[i], gmixer_vol4[i], gmixer_vol5[i],
				gmixer_vol6[i], gmixer_vol7[i],
				gmixer_d1[i], gmixer_d2[i], gmixer_d3[i], gmixer_d4[i], gmixer_d5[i],
				gmixer_d6[i], gmixer_d7[i]
				);
			}
		}
	}
	sbuf = buf;
	return sbuf;
}


// ================


// =======================================================================================
// Collect parameters from Wii or GC
// --------------
/*
std::string ShowAllPB(int a, int i)
{
	if(a == 0)
	{

	}
	else if(a == 1)
	{


	}
	else if(a == 1)
	{

	}
	else if(a == 1)

}
*/
// ================


// =======================================================================================
// Collect parameters from Wii or GC
// --------------

//inline void MixAddVoice(ParamBlockType &pb
//void CollectPB(bool Wii, int i, AXParamBlockWii * PBw, ParamBlockType &pb)
template<class ParamBlockType> void CollectPB(bool Wii, int i, ParamBlockType &PBs)
//void CollectPB(bool Wii, int i, AXParamBlockWii * PBw, AXParamBlock * PBs)
{
	// AXPB base
	gsrc_type[i] = PBs[i].src_type;
	gcoef[i] = PBs[i].coef_select;
	if(Wii) gmixer_control_wii[i] = PBs[i].mixer_control;
		else gmixer_control[i] = PBs[i].mixer_control;

	//running[i] = PBs[i].running;
	gis_stream[i] = PBs[i].is_stream;

	// mixer (some differences)
	gvolume_left[i] = PBs[i].mixer.volume_left;
	gvolume_right[i] = PBs[i].mixer.volume_right;

	gmix_unknown[i] = PBs[i].mixer.unknown;
	gmix_unknown2[i] = PBs[i].mixer.unknown2;

	gcur_volume[i] = PBs[i].vol_env.cur_volume;
	gcur_volume_delta[i] = PBs[i].vol_env.cur_volume_delta;

	gmixer_vol1[i] = PBs[i].mixer.unknown3[0];
	gmixer_vol2[i] = PBs[i].mixer.unknown3[2];
	gmixer_vol3[i] = PBs[i].mixer.unknown3[4];
	gmixer_vol4[i] = PBs[i].mixer.unknown3[6];
	gmixer_vol5[i] = PBs[i].mixer.unknown3[0];
	gmixer_vol6[i] = PBs[i].mixer.unknown3[2];
	gmixer_vol7[i] = PBs[i].mixer.unknown3[4];

	gmixer_d1[i] = PBs[i].mixer.unknown4[1];
	gmixer_d2[i] = PBs[i].mixer.unknown4[3];
	gmixer_d3[i] = PBs[i].mixer.unknown4[5];
	gmixer_d4[i] = PBs[i].mixer.unknown4[7];
	gmixer_d5[i] = PBs[i].mixer.unknown4[1];
	gmixer_d6[i] = PBs[i].mixer.unknown4[3];
	gmixer_d7[i] = PBs[i].mixer.unknown4[5];

	// PBAudioAddr audio_addr
	glooping[i] = PBs[i].audio_addr.looping;
	gaudioFormat[i] = PBs[i].audio_addr.sample_format;
	gloopPos[i]   = (PBs[i].audio_addr.loop_addr_hi << 16) | PBs[i].audio_addr.loop_addr_lo;
	gsampleEnd[i] = (PBs[i].audio_addr.end_addr_hi << 16) | PBs[i].audio_addr.end_addr_lo;
	gsamplePos[i] = (PBs[i].audio_addr.cur_addr_hi << 16) | PBs[i].audio_addr.cur_addr_lo;

	if(gloopPos[i] > 0x20000000) gloopPos[i] -= 0x20000000;
	if(gsampleEnd[i] > 0x20000000) gsampleEnd[i] -= 0x20000000;
	if(gsamplePos[i] > 0x20000000) { gsamplePos[i] -= 0x20000000;
		mem[i] = 2;} else { mem[i] = 1; }

	// PBADPCMLoopInfo adpcm_loop_info (same in GC and Wii)
	gadloop1[i] = PBs[i].adpcm.pred_scale;
	gadloop2[i] = PBs[i].adpcm.yn1;
	gadloop3[i] = PBs[i].adpcm.yn2;

	gloop1[i] = PBs[i].adpcm_loop_info.pred_scale;
	gloop2[i] = PBs[i].adpcm_loop_info.yn1;
	gloop3[i] = PBs[i].adpcm_loop_info.yn2;

	// updates (differences)
	gupdates1[i] = PBs[i].updates.num_updates[0];
	gupdates2[i] = PBs[i].updates.num_updates[1];
	gupdates3[i] = PBs[i].updates.num_updates[2];
	gupdates4[i] = PBs[i].updates.num_updates[3];
	gupdates5[i] = PBs[i].updates.num_updates[4];

	gupdates_addr[i] = (PBs[i].updates.data_hi << 16) | PBs[i].updates.data_lo;
	if(gupdates_addr[i] > 0x80000000 && gupdates_addr[i] < 0x93ffffff)
	{
		gupdates_data[i] = Memory_Read_U32(gupdates_addr[i]);
		gupdates_data1[i] = Memory_Read_U32(gupdates_addr[i] + 4);
		gupdates_data2[i] = Memory_Read_U32(gupdates_addr[i] + 8);
		gupdates_data3[i] = Memory_Read_U32(gupdates_addr[i] + 12);
		gupdates_data3[i] = Memory_Read_U32(gupdates_addr[i] + 16);
	}
		
	// PBSampleRateConverter src

	gratio[i] = (u32)(((PBs[i].src.ratio_hi << 16) + PBs[i].src.ratio_lo) * ratioFactor);
	gratiohi[i] = PBs[i].src.ratio_hi;
	gratiolo[i] = PBs[i].src.ratio_lo;
	gfrac[i] = PBs[i].src.cur_addr_frac;
}
// ===============



// =======================================================================================
// Prepare the condition that makes us show a certain block
// --------------
template<class ParamBlockType>
bool PrepareConditions(bool Wii, int i, ParamBlockType &PBs)
{
	bool Conditions;

	if (m_frame->gOnlyLooping) // show only looping blocks
	{
		Conditions = PBs[i].audio_addr.looping ? true : false;
	}
	else if (m_frame->gShowAll) // show all blocks
	{
		Conditions = true;
	}
	else if (m_frame->giShowAll > -1) // show all blocks
	{
		if (m_frame->giShowAll == 0)
			Conditions = (i < 31);
		else if(m_frame->giShowAll == 1)
			Conditions = (i > 30 && i < 61);
		else if(m_frame->giShowAll == 2)
			Conditions = (i > 60 && i < 91);
		else if(m_frame->giShowAll == 3)
			Conditions = (i > 90 && i < 121);
	}		
	else // show only the ones that have recently been running
	{
		Conditions = (numberRunning.at(i) > 0 || PBs[i].audio_addr.looping);
	}

	return Conditions;
}
// ===============



template<class ParamBlockType>
void Logging_(short* _pBuffer, int _iSize, int a, bool Wii, ParamBlockType &PBs,
			  int numberOfPBs, u32 m_addressPBs)
//void Logging__(short* _pBuffer, int _iSize, int a, bool Wii)
{
	bool Conditions; // Select blocks to show

	// =======================================================================================
	// Update parameter values
	// --------------
	// We could chose to update these only if a block is currently running. Later I'll add options
	// to see both the current and the latest active value.
	int irun = 0;
	for (int i = 0; i < numberOfPBs; i++)
	{
		// --------------------------------------------------------------------
		// Write a line for the text log if nothing is playing
		// --------------
		if (PBs[i].running)
		{
			irun++;
		}
		
		if (i == numberOfPBs - 1 && irun == 0) 
		{
			for (int j = 0; j < nFiles; j++)
			{	
				std::string sfbuff;
				sfbuff = "-----\n";
				aprintf(j, (char *)sfbuff.c_str());
			}
		}
		// --------------


		// --------------------------------------
		// Now go through only a subset of the blocks depending on Conditions
		// ------------------
		/* Prepare conditions. We may for example get Conditions = true for blocks
		   that currently have numberRunning.at(i) > 0 */
		Conditions = PrepareConditions(Wii, i, PBs);
		if (Conditions)
		{
			 // Collect parameters
			CollectPB(Wii, i, PBs);

			// ---------------------------------------------------------------------------------------
			// Write to file
			// --------------
			for (int ii = 0; ii < nFiles; ii++)
			{			 
				std::string sfbuff;
				if(a == 0) sfbuff = "***"; // note if it's before or after an update (*** = before)
					else sfbuff = "   ";

				// write running
				char cbuf[10];
				sprintf(cbuf, "%i", PBs[i].running);
				sfbuff = sfbuff + cbuf;

				sfbuff = sfbuff + writeMessage(ii, i, Wii);

				// write _iSize
				strcpy(cbuf, ""); sprintf(cbuf, "%i", _iSize);
				sfbuff = sfbuff + " | _iSize: " + cbuf + "\n";

				aprintf(ii, (char *)sfbuff.c_str());
			}
			// --------------
		}
	}
	// ==============


	// =======================================================================================
	// Control how often the screen is updated, and then update the screen
	// --------------
	if(a == 0) count1++; // a == 0 when Logging is called before the blocks are updated
	if (m_frame->gUpdFreq > 0 && count1 > (200/m_frame->gUpdFreq))
	{

		// =======================================================================================
		/* Save the displayed running history for each block. Vector1 is a vector1[NUMBER_OF_PBS]
		   [100] vector. */
		// --------------
		/*
		Move all items back like this:		
		1  to  0
		2      1
		3 ...
		5  to  4
		*/
		for (int i = 0; i < numberOfPBs; i++)
		{
			for (int j = 1; j < vectorLength; j++)
			{
				vector1.at(i).at(j-1) = vector1.at(i).at(j);
			}
			// save the latest value
			vector1.at(i).at(vectorLength-1) = PBs[i].running ? true : false;
		}
		// ==============


		// =======================================================================================
		/* Have a separate set for which ones to show. Currently show blocks that have been
		   running at least once the last 100 updates.
		// --------------

		Move all items back like this:		
		1  to  0
		2      1
		3 ...
		*/
		for (int i = 0; i < numberOfPBs; i++)
		{
			for (int j = 1; j < vectorLength2; j++)
			{
				vector2.at(i).at(j-1) = vector2.at(i).at(j);
			}
			// save the latest value
			vector2.at(i).at(vectorLength2-1) = PBs[i].running ? true : false;
		}
		// ==============

		
		// =======================================================================================
		// Count how many we have running now in a certain block
		// --------------
		int jj = 0;
		for (int i = 0; i < numberOfPBs; i++)
		{
			jj = 0;
			for (int j = 0; j < vectorLength2-1; j++) // the hundred last updates
			{
				if (vector2.at(i).at(j)) // if it was on then
				{					
					jj++;					
				}
				numberRunning.at(i) = jj;
			}
		}
		// ==============

			
		// =======================================================================================
		// Write header
		// --------------
		char buffer [1000] = "";
		std::string sbuff;
		sbuff = writeTitle(m_frame->gPreset, Wii);
		// ==============


		// =======================================================================================
		// Now go through all blocks
		// --------------
		for (int i = 0; i < numberOfPBs; i++)
		{
		

		// --------------------------------------
		// Now go through only a subset of the blocks depending on Conditions
		// ------------------
		// Prepare conditions
		Conditions = PrepareConditions(Wii, i, PBs);
		if (Conditions)
		{			
			// --------------------------------------
			// Save playback history text string for the console and GUI debugger
			// ------------------
			if(m_frame)
			{
				std::string guipr; // gui progress

				for (int j = 0; j < vectorLengthGUI; j++)
				{
					if(vector1.at(i).at(j) == false)
					{
						guipr = guipr + "0";
					}
					else
					{
						guipr = guipr + "1";
					}
				}

				u32 run = atoi( guipr.c_str());
				m_frame->m_GPRListView->m_CachedRegs[1][i] = run;
				guipr.clear();
			}

			// and for the console debugger
			for (int j = 0; j < vectorLength; j++)
			{
				if(vector1.at(i).at(j) == false)
				{
					sbuff = sbuff + " ";
				}
				else
				{
					sprintf(buffer, "%c", 177);
					sbuff = sbuff + buffer; strcpy(buffer, "");
				}
			}
			// ---------	


			// Hopefully this is false if we don't have a debugging window and so it doesn't cause a crash
			if(m_frame)
			{
				m_frame->m_GPRListView->m_CachedRegs[2][i] = gsamplePos[i];
				m_frame->m_GPRListView->m_CachedRegs[3][i] = gsampleEnd[i];
				m_frame->m_GPRListView->m_CachedRegs[4][i] = gloopPos[i];

				m_frame->m_GPRListView->m_CachedRegs[5][i] = gvolume_left[i];
				m_frame->m_GPRListView->m_CachedRegs[6][i] = gvolume_right[i];

				m_frame->m_GPRListView->m_CachedRegs[7][i] = glooping[i];
				m_frame->m_GPRListView->m_CachedRegs[8][i] = gloop1[i];
				m_frame->m_GPRListView->m_CachedRegs[9][i] = gloop2[i];
				m_frame->m_GPRListView->m_CachedRegs[10][i] = gloop3[i];

				m_frame->m_GPRListView->m_CachedRegs[11][i] = gis_stream[i];

				m_frame->m_GPRListView->m_CachedRegs[12][i] = gaudioFormat[i];
				m_frame->m_GPRListView->m_CachedRegs[13][i] = gsrc_type[i];
				m_frame->m_GPRListView->m_CachedRegs[14][i] = gcoef[i];

				m_frame->m_GPRListView->m_CachedRegs[15][i] = gfrac[i];

				m_frame->m_GPRListView->m_CachedRegs[16][i] = gratio[i];
				m_frame->m_GPRListView->m_CachedRegs[17][i] = gratiohi[i];
				m_frame->m_GPRListView->m_CachedRegs[18][i] = gratiolo[i];

				m_frame->m_GPRListView->m_CachedRegs[19][i] = gupdates1[i];
				m_frame->m_GPRListView->m_CachedRegs[20][i] = gupdates2[i];
				m_frame->m_GPRListView->m_CachedRegs[21][i] = gupdates3[i];
				m_frame->m_GPRListView->m_CachedRegs[22][i] = gupdates4[i];
				m_frame->m_GPRListView->m_CachedRegs[23][i] = gupdates5[i];
			}

			// add new line
			sbuff = sbuff + writeMessage(m_frame->gPreset, i, Wii); strcpy(buffer, "");
			sbuff = sbuff + "\n";

		} // end of if(Conditions)
		// ==============

		} // end of for (int i = 0; i < numberOfPBs; i++)


		// =======================================================================================
		// Write global values
		// ---------------
		int nOfBlocks;
		int span = m_frame->gLastBlock - m_addressPBs;
		if(Wii)
			nOfBlocks = (m_frame->gLastBlock-m_addressPBs) / 256;
		else
			nOfBlocks = (m_frame->gLastBlock-m_addressPBs) / 192;
		sprintf(buffer, "\nThe parameter blocks span from %08x to %08x (%s bytes) impl. %i blocks | numberOfPBs %i | _iSize %i\n",
			m_addressPBs, m_frame->gLastBlock, ThS(span).c_str(), nOfBlocks, numberOfPBs, _iSize);
		sbuff = sbuff + buffer; strcpy(buffer, "");
		// ===============
			
			
		// =======================================================================================
		// Write settings
		// ---------------
		sprintf(buffer, "\nSettings: SSBM fix %i | SSBM rem1 %i | SSBM rem2 %i\nSequenced %i | Volume %i | Reset %i | Only looping %i | Save file %i\n",
			gSSBM, gSSBMremedy1, gSSBMremedy2, gSequenced,
			gVolume, gReset, m_frame->gOnlyLooping, m_frame->gSaveFile);
		sbuff = sbuff + buffer; strcpy(buffer, "");
		// ===============


		// =======================================================================================
		// Show update frequency
		// ---------------
		sbuff = sbuff + "\n";
		if(!iupdonce)
		{
			viupd.at(0) = 1;
			viupd.at(1) = 1;
			viupd.at(2) = 1;
			iupdonce = true;
		}

		for (u32 i = 0; i < viupd.size(); i++) // 0, 1,..., 9
		{
			if (i < viupd.size()-1)
			{
				viupd.at(viupd.size()-i-1) = viupd.at(viupd.size()-i-2);		// move all forward	
			}
			else
			{
				viupd.at(0) = viupd.at(viupd.size()-1);
			}
			
			// Correction
			if (viupd.at(viupd.size()-3) == 1 && viupd.at(viupd.size()-2) == 1 && viupd.at(viupd.size()-1) == 1)
			{
				viupd.at(0) = 0;
			}
			if(viupd.at(0) == 0 && viupd.at(1) == 1 && viupd.at(2) == 1 && viupd.at(3) == 0)
			{
				viupd.at(0) = 1;
			}				
		}

		for (u32 i = 0; i < viupd.size(); i++)
		{
			if(viupd.at(i) == 0)
				sbuff = sbuff + " ";
			else
				sbuff = sbuff + ".";
		}
		// ================


		// =======================================================================================
		// Print
		// ----------------
		ClearScreen();
		wprintf("%s", sbuff.c_str());
		sbuff.clear(); strcpy(buffer, "");
		// ================

		
		// New values are written so update - DISABLED - It flickered a lot, even worse than a
		// console window. So for now only the console windows is updated.
		/*
		if(m_frame)
		{
			m_frame->NotifyUpdate();
		}
		*/			

		count2=0;
		count1=0;

	} // end of if (j>20)	
	
} // end of function


// I placed this in CUCode_AX so it can share member values with that class
void CUCode_AX::Logging(short* _pBuffer, int _iSize, int a, bool Wii)
{
	/* Doing all this may have a noticable CPU effect, so we can disable it completely
	   this way. But remember that "Save to file" will not write anything then either. */
	if (m_frame->gUpdFreq > 0)
	{
		int version; // AX version
		int numberOfPBs;

		// Declare structures
		AXParamBlock PBs[NUMBER_OF_PBS];
		AXParamBlockWii PBw[NUMBER_OF_PBS];
		AXParamBlockWii_ PBw_[NUMBER_OF_PBS];
		if(_CRC == 0xfa450138) version = 0; else version = 1;
		
		// Read out structs and number of PBs that have data
		if(Wii)
		{
			if(version == 0)
			{
				numberOfPBs = ReadOutPBsWii(m_addressPBs, PBw, NUMBER_OF_PBS);
				Logging_(_pBuffer, _iSize, a, Wii, PBw, numberOfPBs, m_addressPBs);
			}
			else
			{
				numberOfPBs = ReadOutPBsWii(m_addressPBs, PBw_, NUMBER_OF_PBS);
				Logging_(_pBuffer, _iSize, a, Wii, PBw_, numberOfPBs, m_addressPBs);
			}
		}
		else
		{
			numberOfPBs = ReadOutPBs(m_addressPBs, PBs, NUMBER_OF_PBS);
			Logging_(_pBuffer, _iSize, a, Wii, PBs, numberOfPBs, m_addressPBs);
		}	
	}	
}
