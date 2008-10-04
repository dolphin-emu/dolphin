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

#include "../Globals.h"
#include <iostream>
#include <vector>
#include <string> // so that we can test std::string == abc
#ifdef _WIN32
	#include <windows.h>
#endif

#include "../UCodes/UCodes.h"
#include "../UCodes/UCode_AXStructs.h"
#include "../UCodes/UCode_AX.h"

#include "../Debugger/PBView.h"
#include "../Debugger/Debugger.h"
#include "Console.h" // open and close console, clear console window


// Externals

float ratioFactor; // a global to get the ratio factor from MixAdd
int gUpdFreq = 5;
u32 gLastBlock;


// Parameter blocks

std::vector<u32> gloopPos(64);
std::vector<u32> gsampleEnd(64);
std::vector<u32> gsamplePos(64);
// PBSampleRateConverter src
	std::vector<u32> gratio(64);
	std::vector<u32> gratiohi(64);
	std::vector<u32> gratiolo(64);
	std::vector<u32> gfrac(64);
	std::vector<u32> gcoef(64);
// PBSampleRateConverter mixer
	std::vector<u16> gvolume_left(64);
	std::vector<u16> gvolume_right(64);

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




// Counters

int j = 0;
int k = 0;
long int l = 0;
int iupd = 0;
bool iupdonce = false;
std::vector<u16> viupd(15); // the length of the update frequency bar
int vectorLengthGUI = 8; // length of playback history bar for the GUI version
int vectorLength = 15; // for console version


// More stuff

std::vector< std::vector<int> > vector1(64, std::vector<int>(100,0)); 
std::vector<int> numberRunning(64);
std::vector<u16> vector62(vectorLength);
std::vector<u16> vector63(vectorLength);




// Classes

extern CDebugger* m_frame;
		


// I placed this in CUCode_AX because there was some kind of problem to call it otherwise,
// I'm sure it's simple to fix but I couldn't.
void CUCode_AX::Logging(short* _pBuffer, int _iSize, int a)
{

	AXParamBlock PBs[NUMBER_OF_PBS];
	int numberOfPBs = ReadOutPBs(PBs, NUMBER_OF_PBS);


	
	// Control how often the screen is updated
	j++;
	l++;
	if (j > (200/gUpdFreq))
	{

		
		// Move all items back - vector1 is a vector1[64][100] vector, I think
		/*
		Move all items back like this:		
		1  to  2
		2      3
		3 ...
		*/
		for (int i = 0; i < 64; i++)
		{
			for (int j = 1; j < vectorLength; j++)
			{
				vector1.at(i).at(j-1) = vector1.at(i).at(j);
			}
		}
		
		
		// Save the latest value
		
		for (int i = 0; i < numberOfPBs; i++)
		{
			vector1.at(i).at(vectorLength-1) = PBs[i].running;
		}
		
		// =======================================================================================
		// Count how many we have running now
		// --------------
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
		// ==============

		
		// =======================================================================================
		// Write header
		// --------------
		char buffer [1000] = "";
		std::string sbuff;
		sbuff = sbuff + "                Nr    pos / end       lpos     | voll  volr  | isl[pre yn1   yn2] iss | frac  ratio[hi lo]     | 1 2 3 4 5\n";
		// ==============

		
		// go through all running blocks
		for (int i = 0; i < numberOfPBs; i++)
		{
		if (numberRunning.at(i) > 0)
		{

			
			// Playback history for the GUI debugger --------------------------
			if(m_frame)
			{
				std::string guipr; // gui progress

				for (int j = 0; j < vectorLengthGUI; j++)
				{
					if(vector1.at(i).at(j) == 0)
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


			// Playback history for the console debugger --------------------------
			for (int j = 0; j < vectorLength; j++)
			{
				if(vector1.at(i).at(j) == 0)
				{
					//strcat(buffer, " ");
					sbuff = sbuff + " ";
				}
				else
				{
					//sprintf(buffer, "%s%c", buffer, 177); // this will add strange letters if buffer has
					// not been given any string yet

					sprintf(buffer, "%c", 177);
					//strcat(buffer, tmpbuff);

					//strcpy(buffer, "");
					//sprintf(buffer, "%c", 177);
					sbuff = sbuff + buffer; strcpy(buffer, "");
				}
			}
			// ---------	


			// We could chose to update these only if a block is currently running - Later I'll add options
			// to see both the current and the lastets active value.
			//if (PBs[i].running)
			if (true)			
			{
				
			// AXPB base
				gcoef[i] = PBs[i].unknown1;

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
			}

			// hopefully this is false if we don't have a debugging window and so it doesn't cause a crash
			if(m_frame)
			{
				m_frame->m_GPRListView->m_CachedRegs[2][i] = gsamplePos[i];
				m_frame->m_GPRListView->m_CachedRegs[2][i] = gsampleEnd[i];
				m_frame->m_GPRListView->m_CachedRegs[3][i] = gloopPos[i];

				m_frame->m_GPRListView->m_CachedRegs[4][i] = gvolume_left[i];
				m_frame->m_GPRListView->m_CachedRegs[5][i] = gvolume_right[i];

				m_frame->m_GPRListView->m_CachedRegs[6][i] = glooping[i];
				m_frame->m_GPRListView->m_CachedRegs[7][i] = gloop1[i];
				m_frame->m_GPRListView->m_CachedRegs[8][i] = gloop2[i];
				m_frame->m_GPRListView->m_CachedRegs[9][i] = gloop3[i];

				m_frame->m_GPRListView->m_CachedRegs[10][i] = gis_stream[i];

				m_frame->m_GPRListView->m_CachedRegs[11][i] = gaudioFormat[i];
				m_frame->m_GPRListView->m_CachedRegs[12][i] = gsrc_type[i];
				m_frame->m_GPRListView->m_CachedRegs[13][i] = gcoef[i];

				m_frame->m_GPRListView->m_CachedRegs[14][i] = gfrac[i];

				m_frame->m_GPRListView->m_CachedRegs[15][i] = gratio[i];
				m_frame->m_GPRListView->m_CachedRegs[16][i] = gratiohi[i];
				m_frame->m_GPRListView->m_CachedRegs[17][i] = gratiolo[i];

				m_frame->m_GPRListView->m_CachedRegs[18][i] = gupdates1[i];
				m_frame->m_GPRListView->m_CachedRegs[19][i] = gupdates2[i];
				m_frame->m_GPRListView->m_CachedRegs[20][i] = gupdates3[i];
				m_frame->m_GPRListView->m_CachedRegs[21][i] = gupdates4[i];
				m_frame->m_GPRListView->m_CachedRegs[22][i] = gupdates5[i];
			}


			// =======================================================================================
			// PRESETS
			// ---------------------------------------------------------------------------------------
			/*
			/"                Nr    pos / end       lpos     | voll  volr | isl[pre yn1   yn2] iss | frac  ratio[hi lo]     | 1 2 3 4 5\n";
			 "---------------|00 12341234/12341234  12341234 | 00000 00000 | 0[000 00000 00000] 0   | 00000 00000[0 00000]   | 
			*/
			sprintf(buffer,"%c%i %08i/%08i  %08i | %05i %05i | %i[%03i %05i %05i] %i   | %05i %05i[%i %05i]   | %i %i %i %i %i",
				223, i, gsamplePos[i], gsampleEnd[i], gloopPos[i],
				gvolume_left[i], gvolume_right[i],
				glooping[i], gloop1[i], gloop2[i], gloop3[i], gis_stream[i],
				gfrac[i], gratio[i], gratiohi[i], gratiolo[i],
				gupdates1[i], gupdates2[i], gupdates3[i], gupdates4[i], gupdates5[i]
				);
			
			// add new line
			sbuff = sbuff + buffer; strcpy(buffer, "");
			sbuff = sbuff + "\n";


		} // end of if (PBs[i].running)		

		} // end of big loop - for (int i = 0; i < numberOfPBs; i++)




		// =======================================================================================
		// Write global values
		// ---------------
		sprintf(buffer, "\nThe parameter blocks span from %08x to %08x | distance %i %i\n", m_addressPBs, gLastBlock, (gLastBlock-m_addressPBs), (gLastBlock-m_addressPBs) / 192);
		sbuff = sbuff + buffer; strcpy(buffer, "");
		// ===============
			

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
		// ---------------------------------------------------------------------------------------
		ClearScreen();
		// we'll always have one to many \n at the end, remove it
		//wprintf("%s", sbuff.substr(0, sbuff.length()-1).c_str());
		wprintf("%s", sbuff.c_str());

		sbuff.clear(); strcpy(buffer, "");

		
		// New values are written so update - DISABLED - It flickered a lot, even worse than a
		// console window. I'll add a console window later to show the current status.
		//if(m_frame)
		if(false)
		{
			//m_frame->NotifyUpdate();
		}
			

		k=0;
		j=0;

	} // end of if (j>20)
	
	

} // end of function
