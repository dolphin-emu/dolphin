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

bool MicButton = false;
bool IsOpen;

//#define HAVE_PORTAUDIO
#if !defined(HAVE_PORTAUDIO) || !HAVE_PORTAUDIO

void SetMic(bool Value){}

CEXIMic::CEXIMic(int _Index){}
CEXIMic::~CEXIMic(){}
bool CEXIMic::IsPresent() {return false;}
void CEXIMic::SetCS(int cs){}
void CEXIMic::Update(){}
void CEXIMic::TransferByte(u8 &byte){}
bool CEXIMic::IsInterruptSet(){return false;}

#else
//////////////////////////////////////////////////////////////////////////
// We use PortAudio for cross-platform audio input.
// It needs the headers and a lib file for the dll
#include <portaudio.h>

#ifdef _WIN32
#pragma comment(lib, "C:/Users/Shawn/Desktop/portaudio/portaudio-v19/portaudio_x64.lib")
#endif

union InputData
{
	s16 word;
	u8 byte[2];
};

InputData inputData[64]; // 64 words = Max 128 bytes returned????
PaStream *stream;
PaError err;
unsigned short SFreq;
unsigned short SNum;
bool m_bInterruptSet;
bool Sampling;


void SetMic(bool Value)
{
	MicButton = Value;
	if(Sampling)
		Pa_StartStream( stream );
	else
		Pa_StopStream( stream );
}

int patestCallback( const void *inputBuffer, void *outputBuffer,
				   unsigned long frameCount,
				   const PaStreamCallbackTimeInfo* timeInfo,
				   PaStreamCallbackFlags statusFlags,
				   void *userData )
{
	s16 *data = (s16*)inputBuffer;
	//s16 *out = (s16*)outputBuffer;

	if (!m_bInterruptSet && Sampling)
	{
		for(unsigned int i = 0; i < SNum; ++i)
		{
			inputData[i].word = data[i];
			//out[i] = inputData[i].word;
		}
		m_bInterruptSet = true;
	}
	return paContinue;
}

//////////////////////////////////////////////////////////////////////////
// EXI Mic Device
//////////////////////////////////////////////////////////////////////////
CEXIMic::CEXIMic(int _Index)
{
	Index = _Index;

	memset(&inputData, 0 , sizeof(inputData));
	memset(&Status.U16, 0 , sizeof(u16));
	command = 0;
	m_uPosition = 0;
	m_bInterruptSet = false;
	MicButton = false;
	IsOpen = false;
	err = Pa_Initialize();
	if (err != paNoError)
		LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: PortAudio Initialize error %s", Pa_GetErrorText(err));
}

CEXIMic::~CEXIMic()
{
	err = Pa_CloseStream( stream );
	if (err != paNoError)
		LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: PortAudio Close error %s", Pa_GetErrorText(err));
	err = Pa_Terminate();
	if (err != paNoError)
		LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: PortAudio Terminate error %s", Pa_GetErrorText(err));
}

bool CEXIMic::IsPresent() 
{
	return true;
}

void CEXIMic::SetCS(int cs)
{
	if (cs) // not-selected to selected
		m_uPosition = 0;
	else
	{	
		switch (command)
		{
		case cmdID:
		case cmdGetStatus:
		case cmdSetStatus:
		case cmdGetBuffer:
			break;
		case cmdWakeUp:
			// This is probably not a command, but anyway...
			// The command 0xff seems to be used to get in sync with the microphone or to wake it up.
			// Normally, it is issued before any other command, or to recover from errors.
			LOGV(EXPANSIONINTERFACE, 1, "EXI MIC: WakeUp cmd");
			break;
		default:
			LOGV(EXPANSIONINTERFACE, 1, "EXI MIC: unknown CS command %02x\n", command);
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
		m_bInterruptSet = false;
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
		command = byte;	// first byte is command
		byte = 0xFF;	// would be tristate, but we don't care.
	} 
	else
	{
		switch (command)
		{
		case cmdID:
			if (m_uPosition == 1)
				;//byte = 0x80; // dummy cycle - taken from memcard, it doesn't seem to need it here
			else
				byte = (u8)(EXI_DEVTYPE_MIC >> (24-(((m_uPosition-2) & 3) * 8)));
			break;
		case cmdGetStatus:
			{
				if (m_uPosition != 1 && m_uPosition != 2)
					LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: WARNING GetStatus @ pos: %d should never happen", m_uPosition);
				if((!Status.button && MicButton)||(Status.button && !MicButton))
					LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: Mic button %s", MicButton ? "pressed" : "released");

				Status.button = MicButton ? 1 : 0;
				byte = Status.U8[ (m_uPosition - 1) ? 0 : 1];
				LOGV(EXPANSIONINTERFACE, 1, "EXI MIC: Status is    0x%04x", Status.U16);
			}
			break;
		case cmdSetStatus:
			{
				// 0x80 0xXX 0xYY
				// cmd  pos1 pos2

				// Here we assign the byte to the proper place in Status and update portaudio settings
				Status.U8[ (m_uPosition - 1) ? 0 : 1] = byte;

				if(m_uPosition == 2)
				{
					Sampling = (Status.sampling == 1) ? true : false;

					switch (Status.sRate)
					{
					case 0:
						SFreq = 11025;
						break;
					case 1:
						SFreq = 22050;
						break;
					case 2:
						SFreq = 44100;
						break;
					default:
						LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: Trying to set unknown sampling rate");
						SFreq = 44100;
						break;
					}

					switch (Status.pLength)
					{
					case 0:
						SNum = 32;
						break;
					case 1:
						SNum = 64;
						break;
					case 2:
						SNum = 128;
						break;
					default:
						LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: Trying to set unknown period length");
						SNum = 128;
						break;
					}

					LOGV(EXPANSIONINTERFACE, 0, "//////////////////////////////////////////////////////////////////////////");
					LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: Status is now 0x%04x", Status.U16);
					LOGV(EXPANSIONINTERFACE, 0, "\tbutton %i\tsRate %i\tpLength %i\tsampling %i\n",
						Status.button, SFreq, SNum, Status.sampling);

					if(!IsOpen)
					{
						// Open Our PortAudio Stream
						// (shuffle2) This (and the callback) could still be wrong
						err = Pa_OpenDefaultStream(
							&stream,	// Our PaStream
							1,			// Input Channels
							0,			// Output Channels
							paInt16,	// Output format - GC wants PCM samples in signed 16-bit format 
							SFreq,		// Sample Rate
							SNum,		// Period Length (frames per buffer)
							patestCallback,// Our callback!
							NULL);		// Pointer passed to our callback
						if (err != paNoError)
						{
							LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: PortAudio error %s", Pa_GetErrorText(err));
						}
						else
							IsOpen = true;
					}
				}
			}
			break;
		case cmdGetBuffer:
			{
				int pos = m_uPosition - 1;
				// (sonicadvance1)I think if we set the Interrupt to false, it reads another 64
				// Will Look in to it.
				// (shuffle2)Seems like games just continuously get the buffer as long as
				// they're sampling and the mic is generating interrupts
				byte = inputData[pos].byte[ (pos & 1) ? 0 : 1 ];
				LOGV(EXPANSIONINTERFACE, 1, "EXI MIC: GetBuffer%s%d/%d byte: 0x%02x",
					(pos > 9) ? " " : "  ", pos, SNum, byte);
			}
			break;
		default:
			LOGV(EXPANSIONINTERFACE, 0, "EXI MIC: unknown command byte %02x\n", command);
			break;
		}
	}
	m_uPosition++;
}
#endif
