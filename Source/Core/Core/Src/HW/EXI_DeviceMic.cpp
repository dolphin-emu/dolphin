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

#include "Common.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "../Core.h"
#include "../CoreTiming.h"

#include "EXI_Device.h"
#include "EXI_DeviceMic.h"
    
	bool MicButton;
	bool IsOpen;
// Doing it this way since it's Linux only atm due to portaudio, even though the lib is crossplatform
// I had to include libs in the DolphinWX Sconscript file which I thought was BS.
// So I'm committing with all the code ifdeff'ed out
#if 1
void SetMic(bool Value)
{}
bool GetMic()
{return false;}
CEXIMic::CEXIMic(int _Index){}
CEXIMic::~CEXIMic(){}
bool CEXIMic::IsPresent() {return false;}
void CEXIMic::SetCS(int cs){}
void CEXIMic::Update(){}
void CEXIMic::TransferByte(u8 &byte){}
bool CEXIMic::IsInterruptSet(){return false;}
#else

#include <portaudio.h>
#include <stdio.h>

	unsigned char InputData[128*44100]; // Max Data is 128 samples at 44100
	PaStream *stream;
	PaError err;
	unsigned short SFreq;
	unsigned short SNum;
	unsigned int Sample;
	bool m_bInterruptSet;
		bool Sampling;


void SetMic(bool Value)
{
	if(Value != MicButton)
	{
		MicButton = Value;
		printf("Mic is set to %s\n", MicButton ? "true" : "false");
		if(Sampling)
		{
			if(MicButton)
				Pa_StartStream( stream );
			else
				Pa_StopStream( stream );
		}
	}
}
bool GetMic()
{
	return MicButton;
}
static unsigned int k = 0;
int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
   unsigned char *data = (unsigned char*)inputBuffer; 
    unsigned int i;
    
    for( i=0; i<framesPerBuffer && k < (SFreq*SNum); i++, k++ )
    {
        InputData[k] = data[i];
    }
    m_bInterruptSet = true;
    return 0;
}

CEXIMic::CEXIMic(int _Index)
{
	Index = _Index;
 
	command = 0;
	Sample = 0;
	m_uPosition = 0;
	formatDelay = 0;
	ID = 0x0a000000;
	m_bInterruptSet = false;
	MicButton = false;
	IsOpen = false;
	Pa_Initialize();

}


CEXIMic::~CEXIMic()
{
	Pa_CloseStream( stream );
	Pa_Terminate();
}

bool CEXIMic::IsPresent() 
{
	return true;
}

void CEXIMic::SetCS(int cs)
{
	if (cs)  // not-selected to selected
	{
		m_uPosition = 0;
		m_bInterruptSet = false;
	}
	else
	{	
		switch (command)
		{
			default:
				//printf("Don't know Command %x\n", command);
			break;
		}
	}
}

void CEXIMic::Update()
{
}

bool CEXIMic::IsInterruptSet()
{
	if(m_bInterruptSet)
	{
		//m_bInterruptSet = false;
		return true;
	}
	else
	{
		return false;
	}
}

void CEXIMic::TransferByte(u8 &byte)
{
	if (m_uPosition == 0)
	{
		command = byte;  // first byte is command
		byte = 0xFF; // would be tristate, but we don't care.

		if(command == cmdClearStatus)
		{
			byte = 0xFF;
			m_uPosition = 0;
			m_bInterruptSet = false;
		}
	} 
	else
	{
		switch (command)
		{
		case cmdID:
			if (m_uPosition == 1)
				;//byte = 0x80; // dummy cycle
			else
				byte = (u8)(ID >> (24-(((m_uPosition-2) & 3) * 8)));
		break;
		// Setting Bits: REMEMBER THIS! D:<
		// <ector--> var |= (1 << bitnum_from_0)
		// <ector--> var &= ~(1 << bitnum_from_0) clears
		case cmdStatus:
		{
			if(GetMic())
			{
				Status.U16 |= (1 << 7);
			}
			else
			{
				Status.U16 &= ~(1 << 7);
			}
			byte = (u8)(Status.U16 >> (24-(((m_uPosition-2) & 3) * 8)));
			
		}
		break;
		case cmdSetStatus:
		{
			Status.U8[ (m_uPosition - 1) ? 0 : 1] = byte;
			if(m_uPosition == 2)
			{
				printf("Status is 0x%04x ", Status.U16);
				//Status is 0x7273 1 1 0 0 1 1 1 0\ 0 1 0 0 1 1 1 0
				//Status is 0x4b00 
				// 0 0 0 0 0 0 0 0	: Bit 0-7:	Unknown
				// 1	: Bit 8		: 1 : Button Pressed 
				// 1	: Bit 9		: 1 ? Overflow? 
				// 0	: Bit 10	: Unknown related to 0 and 15 values It seems
				// 1 0	: Bit 11-12	: Sample Rate, 00-11025, 01-22050, 10-44100, 11-??
				// 0 1	: Bit 13-14	: Period Length, 00-32, 01-64, 10-128, 11-??? 
				// 0	: Bit 15	: If We Are Sampling or Not 
				
				if((Status.U16 >> 15) & 1) // We ARE Sampling
				{
					printf("We are now Sampling");
					Sampling = true;
				}
				else
				{
					Sampling = false;
					// Only set to false once we have run out of Data?
					//m_bInterruptSet = false;
				}
				if(!(Status.U16 >> 11) & 1)
					if((Status.U16 >> 12) & 1 )
						SFreq = 22050;
					else
						SFreq = 11025;
				else
					SFreq = 44100;
					
				if(!(Status.U16 >> 13) & 1)
					if((Status.U16 >> 14) & 1)
						SNum = 64;
					else
						SNum = 32;
				else
					SNum = 128;
			
				for(int a = 0;a < 16;a++)
					printf("%d ", (Status.U16 >> a) & 1);
				printf("\n");
				if(!IsOpen)
				{
					// Open Our PortAudio Stream
    				err = Pa_OpenDefaultStream( &stream,
                                1,         
                                0,          
                                paUInt8 , 
                                SFreq,
                                SNum,        
                                patestCallback, 
                                NULL); 
   					 if( err != paNoError )
   					 	printf("error %s\n", Pa_GetErrorText (err));
   					 IsOpen = true;
				}
			}
		}
		break;
		case cmdGetBuffer:
			static unsigned long At = 0;
			printf("POS %d\n", m_uPosition);
			// Are we not able to return all the data then?
			// I think if we set the Interrupt to false, it reads another 64
			// Will Look in to it.
			// Set to False here? Prevents lock ups maybe?
			if(At >= SNum){
				At = 0;
				k = 0;
				m_bInterruptSet = false;
			}
			byte = InputData[At];
			At++;
		break;
		default:
			printf("Don't know command %x in Byte transfer\n", command);
		break;
		}
	}
	m_uPosition++;
}
#endif
