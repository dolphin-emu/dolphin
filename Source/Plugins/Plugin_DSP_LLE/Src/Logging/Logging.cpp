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


#ifdef _WIN32


// =======================================================================================
// Includes
// --------------
#include <iostream>
#include <vector>
#include <string> // So that we can test if std::string == abc

#include "Common.h"

#include "UCode_AXStructs.h" // they are only in a virtual dir called UCodes AX
#include "Console.h" // For wprintf, ClearScreen

// ---------------------------------------------------------------------------------------
// Declarations
// --------------
#define NUMBER_OF_PBS 64 // Todo: move this to a logging class



// ---------------------------------------------------------------------------------------
// Externals
// --------------
extern u32 m_addressPBs;
float ratioFactor;
int globaliSize;
short globalpBuffer;
u32 gLastBlock;
// --------------


// ---------------------------------------------------------------------------------------
// Vectors and other stuff
// --------------
std::vector<u32> gloopPos(64);
std::vector<u32> gsampleEnd(64);
std::vector<u32> gsamplePos(64);
	std::vector<u32> gratio(64);
	std::vector<u32> gratiohi(64);
	std::vector<u32> gratiolo(64);
	std::vector<u32> gfrac(64);
	std::vector<u32> gcoef(64);

// PBSampleRateConverter mixer
	std::vector<u16> gvolume_left(64);
	std::vector<u16> gvolume_right(64);
	std::vector<u16> gmixer_control(64);
	std::vector<u16> gcur_volume(64);
	std::vector<u16> gcur_volume_delta(64);


	std::vector<u16> gaudioFormat(64);
	std::vector<u16> glooping(64);
	std::vector<u16> gsrc_type(64);
	std::vector<u16> gis_stream(64);

// loop
	std::vector<u16> gloop1(64);
	std::vector<u16> gloop2(64);
	std::vector<u16> gloop3(64);
	std::vector<u16> gadloop1(64);
	std::vector<u16> gadloop2(64);
	std::vector<u16> gadloop3(64);

// updates
	std::vector<u16> gupdates1(64);
	std::vector<u16> gupdates2(64);
	std::vector<u16> gupdates3(64);
	std::vector<u16> gupdates4(64);
	std::vector<u16> gupdates5(64);
	std::vector<u32> gupdates_addr(64);

// other stuff
std::vector<u16> Jump(64); // this is 1 or 0
std::vector<int> musicLength(64);
std::vector< std::vector<int> > vector1(64, std::vector<int>(100,0)); 
std::vector<int> numberRunning(64);

int j = 0;
int k = 0;
__int64 l = 0;
int iupd = 0;
bool iupdonce = false;
std::vector<u16> viupd(15); // the length of the update frequency bar
int vectorLength = 15; // the length of the playback history bar and how long
// old blocks are shown

std::vector<u16> vector62(vectorLength);
std::vector<u16> vector63(vectorLength);

int ReadOutPBs(AXParamBlock * _pPBs, int _num);
// ===========


// =======================================================================================
// Main logging function
// --------------
void Logging()
{
	// ---------------------------------------------------------------------------------------


	// ---------------------------------------------------------------------------------------
	// Control how often the screen is updated
	j++;
	l++;
	if (j>1000000) // TODO: make the update frequency adjustable from the logging window
	{
		
		AXParamBlock PBs[NUMBER_OF_PBS];
		int numberOfPBs = ReadOutPBs(PBs, NUMBER_OF_PBS);

		// =======================================================================================
		// Vector1 is a vector1[64][100] vector
		/*
		Move all items back like this
		1  to  2
		2      3
		3  ...
		*/
		// ----------------
		for (int i = 0; i < 64; i++)
		{
			for (int j = 1; j < vectorLength; j++)
			{
				vector1.at(i).at(j-1) = vector1.at(i).at(j);
			}
		}
		// =================

		// ---------------------------------------------------------------------------------------
		// Enter the latest value	
		for (int i = 0; i < numberOfPBs; i++)
		{
			vector1.at(i).at(vectorLength-1) = PBs[i].running;
		}
		// -----------------


		// ---------------------------------------------------------------------------------------
		// Count how many blocks we have running now
		int jj = 0;
		for (int i = 0; i < 64; i++)
		{
			for (int j = 0; j < vectorLength-1; j++)
			{
				if (vector1.at(i).at(j) == 1)
				{
					jj++;					
				}
				numberRunning.at(i) = jj;
			}
		}
		// --------------


		// ---------------------------------------------------------------------------------------
		// Write the first row
		char buffer [1000] = "";
		std::string sbuff;
		//sbuff = sbuff + "          Nr |                       | frac  ratio    |     old              new  \n"; // 5
		sbuff = sbuff + "                Nr    pos / end       lpos     | voll  volr  curv  vold mix   | isl[pre yn1   yn2] iss | frac  ratio[hi lo]     | 1 2 3 4 5\n";
		// --------------


		// ---------------------------------------------------------------------------------------
		// Read out values for all blocks
		for (int i = 0; i < numberOfPBs; i++)
		{
		if (numberRunning.at(i) > 0)
		{
			// =======================================================================================
			// Write the playback bar
			// -------------
			for (int j = 0; j < vectorLength; j++)
			{
				if(vector1.at(i).at(j) == 0)
				{
					sbuff = sbuff + " ";
				}
				else
				{
					sprintf(buffer, "%c", 177);
					sbuff = sbuff + buffer; strcpy(buffer, "");
				}
			}
			// ==============
			

			// ================================================================================================
			int sampleJump;
			int loopJump;
			//if (PBs[i].running && PBs[i].adpcm_loop_info.yn1 && PBs[i].mixer.volume_left)
			if (true)			
			{
				// ---------------------------------------------------------------------------------------
				// AXPB base
				//int running = pb.running;
					gcoef[i] = PBs[i].unknown1;

				sampleJump = ((PBs[i].audio_addr.cur_addr_hi << 16) | PBs[i].audio_addr.cur_addr_lo) - gsamplePos[i];
				loopJump = ((PBs[i].audio_addr.loop_addr_hi << 16) | PBs[i].audio_addr.loop_addr_lo) - gloopPos[i];

				gloopPos[i]   = (PBs[i].audio_addr.loop_addr_hi << 16) | PBs[i].audio_addr.loop_addr_lo;
				gsampleEnd[i] = (PBs[i].audio_addr.end_addr_hi << 16) | PBs[i].audio_addr.end_addr_lo;
				gsamplePos[i] = (PBs[i].audio_addr.cur_addr_hi << 16) | PBs[i].audio_addr.cur_addr_lo;

				// PBSampleRateConverter src

					gratio[i] = (u32)(((PBs[i].src.ratio_hi << 16) + PBs[i].src.ratio_lo) * ratioFactor);
					gratiohi[i] = PBs[i].src.ratio_hi;
					gratiolo[i] = PBs[i].src.ratio_lo;
					gfrac[i] = PBs[i].src.cur_addr_frac;

				// adpcm_loop_info
					gadloop1[i] = PBs[i].adpcm.pred_scale;
					gadloop2[i] = PBs[i].adpcm.yn1;
					gadloop3[i] = PBs[i].adpcm.yn2;

					gloop1[i] = PBs[i].adpcm_loop_info.pred_scale;
					gloop2[i] = PBs[i].adpcm_loop_info.yn1;
					gloop3[i] = PBs[i].adpcm_loop_info.yn2;

				// updates
					gupdates1[i] = PBs[i].updates.num_updates[0];
					gupdates2[i] = PBs[i].updates.num_updates[1];
					gupdates3[i] = PBs[i].updates.num_updates[2];
					gupdates4[i] = PBs[i].updates.num_updates[3];
					gupdates5[i] = PBs[i].updates.num_updates[4];

					gupdates_addr[i] = (PBs[i].updates.data_hi << 16) | PBs[i].updates.data_lo;

				gaudioFormat[i] = PBs[i].audio_addr.sample_format;
				glooping[i] = PBs[i].audio_addr.looping;
				gsrc_type[i] = PBs[i].src_type;
				gis_stream[i] = PBs[i].is_stream;

				// mixer
					gvolume_left[i] = PBs[i].mixer.volume_left;
					gvolume_right[i] = PBs[i].mixer.volume_right;

					gmixer_control[i] = PBs[i].mixer_control;
					gcur_volume[i] = PBs[i].vol_env.cur_volume;
					gcur_volume_delta[i] = PBs[i].vol_env.cur_volume_delta;
				
				// other stuff
				Jump[i] = (gfrac[i] >> 16); // This is 1 or 0
				musicLength[i] = gsampleEnd[i] - gloopPos[i];
			}

			// ================================================================================================


				
			// =======================================================================================
			// PRESETS
			// ---------------------------------------------------------------------------------------
			/*
			/"                Nr    pos / end       lpos     | voll  volr  curv  vold mix   | isl[pre yn1   yn2] iss | frac  ratio[hi lo]     | 1 2 3 4 5\n";
			 "---------------|00 12341234/12341234  12341234 | 00000 00000 00000 0000 00000 | 0[000 00000 00000] 0   | 00000 00000[0 00000]   | 
			*/
			sprintf(buffer,"%c%i %08i/%08i  %08i | %05i %05i %05i %04i %05i | %i[%03i %05i %05i] %i   | %05i %05i[%i %05i]   | %i %i %i %i %i",
				223, i, gsamplePos[i], gsampleEnd[i], gloopPos[i],
				gvolume_left[i], gvolume_right[i], gcur_volume[i], gcur_volume_delta[i], gmixer_control[i],
				glooping[i], gloop1[i], gloop2[i], gloop3[i], gis_stream[i],
				gfrac[i], gratio[i], gratiohi[i], gratiolo[i],
				gupdates1[i], gupdates2[i], gupdates3[i], gupdates4[i], gupdates5[i]
				);
			// =======================================================================================

			// write a new line
			sbuff = sbuff + buffer; strcpy(buffer, "");
			sbuff = sbuff + "\n";

		} // end of if (true)
		

		} // end of big loop - for (int i = 0; i < numberOfPBs; i++)

			
		// =======================================================================================
		// Write global values
		sprintf(buffer, "\nParameter blocks span from %08x | to %08x | distance %i %i\n", m_addressPBs, gLastBlock, (gLastBlock-m_addressPBs), (gLastBlock-m_addressPBs) / 192);
		sbuff = sbuff + buffer; strcpy(buffer, "");
		// ==============

			
		// =======================================================================================
		// Show update frequency
		// ---------------
		sbuff = sbuff + "\n";
		if(!iupdonce)
		{
			/*
			for (int i = 0; i < 10; i++)
			{
				viupd.at(i) == 0;
			}
			*/
			viupd.at(0) = 1;
			viupd.at(1) = 1;
			viupd.at(2) = 1;
			iupdonce = true;
		}

		for (int i = 0; i < viupd.size(); i++) // 0, 1,..., 9
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

		for (int i = 0; i < viupd.size(); i++)
		{
			if(viupd.at(i) == 0)
				sbuff = sbuff + " ";
			else
				sbuff = sbuff + ".";
		}
		// ================


		// =======================================================================================
		// Print
		// ---------------
		ClearScreen();
		wprintf("%s", sbuff.c_str());
		sbuff.clear(); strcpy(buffer, "");		
		// ---------------
		k=0;
		j=0;
		// ---------------
	}
	
	// ---------------------------------------------------------------------------------------

}
// =======================================================================================





#endif