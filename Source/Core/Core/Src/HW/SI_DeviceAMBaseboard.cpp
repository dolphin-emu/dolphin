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


#include "SI.h"
#include "SI_Device.h"
#include "SI_DeviceAMBaseboard.h"

#include "GCPadStatus.h"
#include "GCPad.h"
#include "..\ConfigManager.h"

// where to put baseboard debug
#define AMBASEBOARDDEBUG OSREPORT

// "JAMMA Video Standard" I/O
class JVSIOMessage
{
public:
	int m_ptr, m_last_start, m_csum;
	unsigned char m_msg[0x80];

	JVSIOMessage()
	{
		m_ptr = 0;
		m_last_start = 0;
	}

	void start(int node)
	{
		m_last_start = m_ptr;
		unsigned char hdr[3] = {0xe0, (unsigned char)node, 0};
		m_csum = 0;
		addData(hdr, 3, 1);
	}
	void addData(const void *data, size_t len)
	{
		addData((const unsigned char*)data, len);
	}
	void addData(const char *data)
	{
		addData(data, strlen(data));
	}
	void addData(int n)
	{
		unsigned char cs = n;
		addData(&cs, 1);
	}

	void end()
	{
		int len = m_ptr - m_last_start;
		m_msg[m_last_start + 2] = len - 2; // assuming len <0xD0
		addData(m_csum + len - 2);
	}

	void addData(const unsigned char *dst, size_t len, int sync = 0)
	{
		while (len--)
		{
			int c = *dst++;
			if (!sync && ((c == 0xE0) || (c == 0xD0)))
			{
				m_msg[m_ptr++] = 0xD0;
				m_msg[m_ptr++] = c - 1;
			} else
				m_msg[m_ptr++] = c;
			if (!sync)
				m_csum += c;
			sync = 0;
			if (m_ptr >= 0x80)
				PanicAlert("JVSIOMessage overrun!");
		}
	}
}; // end class JVSIOMessage


// AM-Baseboard device on SI
CSIDevice_AMBaseboard::CSIDevice_AMBaseboard(SIDevices device, int _iDeviceNumber)
	: ISIDevice(device, _iDeviceNumber)
{
	memset(coin, 0, sizeof(coin));
}

/*	MKGP controls mapping:
	stickX	- steering
	triggerRight - gas
	triggerLeft - brake
	A		- Item button
	B		- Cancel button
	Z		- Coin
	Y		- Test mode (not working)
	X		- Service

	VS2002 controls mapping:
	D-pad	- movement
	B		- Short pass
	A		- Long pass
	X		- Shoot
	Z		- Coin
	Y		- Test mode
	triggerRight - Service
*/
int CSIDevice_AMBaseboard::RunBuffer(u8* _pBuffer, int _iLength)
{
	// for debug logging only
	ISIDevice::RunBuffer(_pBuffer, _iLength);

	int iPosition = 0;
	while(iPosition < _iLength)
	{	
		// read the command
		EBufferCommands command = static_cast<EBufferCommands>(_pBuffer[iPosition ^ 3]);
		iPosition ++;

		// handle it
		switch(command)
		{
		case CMD_RESET: // returns ID and dip switches
			{
				*(u32*)&_pBuffer[0] = SI_AM_BASEBOARD|0x100; // 0x100 is progressive flag according to dip switch
				iPosition = _iLength; // break the while loop
			}
			break;
		case CMD_GCAM: 
			{
				int i;

				// calculate checksum over buffer
				int csum = 0;
				for (i=0; i<_iLength; ++i)
					csum += _pBuffer[i];

				unsigned char res[0x80];
				int resp = 0;

				int real_len = _pBuffer[1^3];
				int p = 2;

				static int d10_1 = 0xfe;

				memset(res, 0, 0x80);
				res[resp++] = 1;
				res[resp++] = 1;

#define ptr(x) _pBuffer[(p + x)^3]
				while (p < real_len+2)
				{
					switch (ptr(0))
					{
					case 0x10:
						{
							DEBUG_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 10, %02x (READ STATUS&SWITCHES)", ptr(1));
							SPADStatus PadStatus;
							memset(&PadStatus, 0 ,sizeof(PadStatus));
							Pad::GetStatus(ISIDevice::m_iDeviceNumber, &PadStatus);
							res[resp++] = 0x10;
							res[resp++] = 0x2;
							int d10_0 = 0xdf;

							/* baseboard test/service switches ???, disabled for a while
							if (PadStatus.button & PAD_BUTTON_Y)	// Test
								d10_0 &= ~0x80;
							if (PadStatus.button & PAD_BUTTON_X)	// Service
								d10_0 &= ~0x40;
							*/

							res[resp++] = d10_0;
							res[resp++] = d10_1;
							break;
						}
					case 0x11:
						{
							ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 11, %02x (READ SERIAL NR)", ptr(1));
							char string[] = "AADE-01A14964511";
							res[resp++] = 0x11;
							res[resp++] = 0x10;
							memcpy(res + resp, string, 0x10);
							resp += 0x10;
							break;
						}
					case 0x12:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 12, %02x %02x", ptr(1), ptr(2));
						res[resp++] = 0x12;
						res[resp++] = 0x00;
						break;
					case 0x15:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 15, %02x (READ FIRM VERSION)", ptr(1));
						res[resp++] = 0x15;
						res[resp++] = 0x02;
						res[resp++] = 0x00;
						res[resp++] = 0x29; // FIRM VERSION
						break;
					case 0x16:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 16, %02x (READ FPGA VERSION)", ptr(1));
						res[resp++] = 0x16;
						res[resp++] = 0x02;
						res[resp++] = 0x07;
						res[resp++] = 0x06; // FPGAVERSION
						/*
						res[resp++] = 0x16;
						res[resp++] = 0x00;
						p += 2;
						*/
						break;
					case 0x1f:
						{
							ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 1f, %02x %02x %02x %02x %02x (REGION)", ptr(1), ptr(2), ptr(3), ptr(4), ptr(5));
							unsigned char string[] =  
								"\x00\x00\x30\x00"
								//"\x01\xfe\x00\x00" // JAPAN
								"\x02\xfd\x00\x00" // USA
								//"\x03\xfc\x00\x00" // export
								"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
							res[resp++] = 0x1f;
							res[resp++] = 0x14;

							for (i=0; i<0x14; ++i)
								res[resp++] = string[i];
							break;
						}
					case 0x31:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 31 (UNKNOWN)");
						res[resp++] = 0x31;
						res[resp++] = 0x02;
						res[resp++] = 0x00;
						res[resp++] = 0x00;
						break;
					case 0x32:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 32 (UNKNOWN)");
						res[resp++] = 0x32;
						res[resp++] = 0x02;
						res[resp++] = 0x00;
						res[resp++] = 0x00;
						break;
					case 0x40:
					case 0x41:
					case 0x42:
					case 0x43:
					case 0x44:
					case 0x45:
					case 0x46:
					case 0x47:
					case 0x48:
					case 0x49:
					case 0x4a:
					case 0x4b:
					case 0x4c:
					case 0x4d:
					case 0x4e:
					case 0x4f:
						{
							DEBUG_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD %02x, %02x %02x %02x %02x %02x %02x %02x (JVS IO)", 
								ptr(0), ptr(1), ptr(2), ptr(3), ptr(4), ptr(5), ptr(6), ptr(7));
							int pptr = 2;
							JVSIOMessage msg;

							msg.start(0);
							msg.addData(1);

							unsigned char jvs_io_buffer[0x80];
							int nr_bytes = ptr(pptr + 2); // byte after e0 xx
							int jvs_io_length = 0;
							for (i=0; i<nr_bytes + 3; ++i)
								jvs_io_buffer[jvs_io_length++] = ptr(pptr + i);
							int node = jvs_io_buffer[1];

							unsigned char *jvs_io = jvs_io_buffer + 3;
							jvs_io_length--; // checksum
							while (jvs_io < (jvs_io_buffer + jvs_io_length))
							{

								int cmd = *jvs_io++;
								int unknown = 0;
								DEBUG_LOG(AMBASEBOARDDEBUG, "JVS IO, node=%d, cmd=%02x", node, cmd);

								switch (cmd)
								{
								case 0x10: // get ID
									msg.addData(1);
									if (!memcmp(SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), "RELSAB", 6))
										msg.addData("namco ltd.;FCA-1;Ver1.01;JPN,Multipurpose + Rotary Encoder");
									else
										msg.addData("SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551;Ver1.00");
									msg.addData(0);
									break;
								case 0x11: // cmd revision
									msg.addData(1);
									msg.addData(0x11);
									break;
								case 0x12: // jvs revision
									msg.addData(1);
									msg.addData(0x20);
									break;
								case 0x13: // com revision
									msg.addData(1);
									msg.addData(0x10);
									break;
								case 0x14: // get features
									msg.addData(1);
									if (!memcmp(SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), "RELSAB", 6)) {
										msg.addData((void *)"\x01\x01\x13\x00", 4);  // 1 player, 19 bit
										msg.addData((void *)"\x02\x02\x00\x00", 4);  // 2 coin slots
										msg.addData((void *)"\x03\x08\x00\x00", 4);  // 8 analogs
										msg.addData((void *)"\x12\x0c\x00\x00", 4);  // 12bit out
										msg.addData((void *)"\x00\x00\x00\x00", 4);
									} else {
										msg.addData((void *)"\x01\x02\x0d\x00", 4);  // 2 players, 13 bit
										msg.addData((void *)"\x02\x02\x00\x00", 4);  // 2 coin slots
										msg.addData((void *)"\x03\x08\x00\x00", 4);  // 8 analogs
										msg.addData((void *)"\x12\x06\x00\x00", 4);  // 6bit out
										msg.addData((void *)"\x00\x00\x00\x00", 4);
									}
									break;
								case 0x15: // baseboard id
									while (*jvs_io++) {};
									msg.addData(1);
									break;
								case 0x20: // buttons
									{
										int nr_players = *jvs_io++;
										int bytes_per_player = *jvs_io++;
										int j;
										msg.addData(1);

										SPADStatus PadStatus;
										Pad::GetStatus(0, &PadStatus);
											if (PadStatus.button & PAD_BUTTON_Y)	// Test button
												msg.addData(0x80);
											else
												msg.addData(0x00);
										for (i=0; i<nr_players; ++i)
										{
											SPADStatus PadStatus;
											Pad::GetStatus(i, &PadStatus);
											unsigned char player_data[3] = {0,0,0};
											if (!memcmp(SConfig::GetInstance().m_LocalCoreStartupParameter.GetUniqueID().c_str(), "RELSAB", 6)) {
												if (PadStatus.button & PAD_BUTTON_START)	// Not used in MKGP
													player_data[0] |= 0x80;
												if (PadStatus.button & PAD_BUTTON_X)	// Service button
													player_data[0] |= 0x40;
												// Not used in MKGP
												if (PadStatus.button & PAD_BUTTON_UP)
													player_data[0] |= 0x20;
												if (PadStatus.button & PAD_BUTTON_DOWN)
													player_data[0] |= 0x10;
												if (PadStatus.button & PAD_BUTTON_LEFT)
													player_data[0] |= 0x08;
												if (PadStatus.button & PAD_BUTTON_RIGHT)
													player_data[0] |= 0x04;

												if (PadStatus.button & PAD_BUTTON_A)	// Item button
													player_data[1] |= 0x20;
												if (PadStatus.button & PAD_BUTTON_B)	// Cancel button
													player_data[1] |= 0x10;
											} else {
												if (PadStatus.button & PAD_BUTTON_START)
													player_data[0] |= 0x80;
												if (PadStatus.button & PAD_TRIGGER_R)	// Service button
													player_data[0] |= 0x40;
												if (PadStatus.button & PAD_BUTTON_UP)
													player_data[0] |= 0x20;
												if (PadStatus.button & PAD_BUTTON_DOWN)
													player_data[0] |= 0x10;
												if (PadStatus.button & PAD_BUTTON_LEFT)
													player_data[0] |= 0x08;
												if (PadStatus.button & PAD_BUTTON_RIGHT)
													player_data[0] |= 0x04;
												if (PadStatus.button & PAD_BUTTON_A)
													player_data[0] |= 0x02;
												if (PadStatus.button & PAD_BUTTON_X)
													player_data[0] |= 0x01;

												if (PadStatus.button & PAD_BUTTON_B)
													player_data[1] |= 0x80;
											}
											for (j=0; j<bytes_per_player; ++j)
												msg.addData(player_data[j]);
										}
										break;
									}
								case 0x21: // coins
								{
									SPADStatus PadStatus;
									int slots = *jvs_io++;
									msg.addData(1);
									for (i = 0; i < slots; i++)	{
										Pad::GetStatus(i, &PadStatus);
										if ((PadStatus.button & PAD_TRIGGER_Z) && !coin_pressed[i])	{
											coin[i]++;
										}
										coin_pressed[i]=PadStatus.button & PAD_TRIGGER_Z;
										msg.addData((coin[i]>>8)&0x3f);
										msg.addData(coin[i]&0xff);
									}
									//dbgprintf("JVS-IO:Get Coins Slots:%u Unk:%u\n", slots, unk );
								} break;
								case 0x22: // analogs
								{
									msg.addData(1);	// status
									int analogs = *jvs_io++;
									SPADStatus PadStatus;
									Pad::GetStatus(0, &PadStatus);

									// 8 bit to 16 bit conversion
									msg.addData(PadStatus.stickX);	// steering
									msg.addData(PadStatus.stickX);

									// 8 bit to 16 bit conversion
									msg.addData(PadStatus.triggerRight);	// gas
									msg.addData(PadStatus.triggerRight);

									// 8 bit to 16 bit conversion
									msg.addData(PadStatus.triggerLeft);	// brake
									msg.addData(PadStatus.triggerLeft);

									for( i=0; i < (analogs - 3); i++ ) {
										msg.addData( 0 );
										msg.addData( 0 );
									}
								} break;
								case 0x30:	// sub coins
								{
									int slot = *jvs_io++;
									coin[slot]-= (*jvs_io++<<8)|*jvs_io++;
									msg.addData(1);
								} break;
								case 0x32:	// General out
								{
									int bytes = *jvs_io++;
									while (bytes--) {*jvs_io++;}
									msg.addData(1);
								} break;
								case 0x35: // add coins
								{
									int slot = *jvs_io++;
									coin[slot]+= (*jvs_io++<<8)|*jvs_io++;
									msg.addData(1);
								} break;
								case 0x70: // custom namco's command subset
								{
									int cmd = *jvs_io++;
									if (cmd == 0x18) { // id check
										jvs_io+=4;
										msg.addData(1);
										msg.addData(0xff);
									} else {
										msg.addData(1);
										///dbgprintf("JVS-IO:Unknown\n");
									}
								} break;
								case 0xf0:
									if (*jvs_io++ == 0xD9)
									{
										ERROR_LOG(AMBASEBOARDDEBUG, "JVS RESET");
									} else
										unknown = 1;
									msg.addData(1);

									d10_1 |= 1;
									break;
								case 0xf1:
									node = *jvs_io++;
									ERROR_LOG(AMBASEBOARDDEBUG, "JVS SET ADDRESS, node=%d", node);
									msg.addData(node == 1);
									d10_1 &= ~1;
									break;
								default:
									break;
								}

								pptr += jvs_io_length;

							}

							msg.end();

							res[resp++] = ptr(0);

							unsigned char *buf = msg.m_msg;
							int len = msg.m_ptr;
							res[resp++] = len;
							for (i=0; i<len; ++i)
								res[resp++] = buf[i];
							break;
						}
					case 0x60:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 60, %02x %02x %02x", ptr(1), ptr(2), ptr(3));
						break;
					default:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD %02x (unknown) %02x %02x %02x %02x %02x", ptr(0), ptr(1), ptr(2), ptr(3), ptr(4), ptr(5));
						break;
					}
					p += ptr(1) + 2;
				}
				memset(_pBuffer, 0, _iLength);

				int len = resp - 2;

				p = 0;
				res[1] = len;
				csum = 0;
				char logptr[1024];
				char *log = logptr;
				for (i=0; i<0x7F; ++i)
				{
					csum += ptr(i) = res[i];
					log += sprintf(log, "%02x ", ptr(i));
				}
				ptr(0x7f) = ~csum;
				DEBUG_LOG(AMBASEBOARDDEBUG, "command send back: %s", logptr);
#undef ptr


				// (tmbinc) hotfix: delay output by one command to work around their broken parser. this took me a month to find. ARG!
				static unsigned char last[2][0x80];
				static int lastptr[2];

				{
					memcpy(last + 1, _pBuffer, 0x80);
					memcpy(_pBuffer, last, 0x80);
					memcpy(last, last + 1, 0x80);

					lastptr[1] = _iLength;
					_iLength = lastptr[0];
					lastptr[0] = lastptr[1];
				}

				iPosition = _iLength;
				break;
			}
			// DEFAULT
		default:
			{
				ERROR_LOG(SERIALINTERFACE, "unknown SI command     (0x%x)", command);
				PanicAlert("SI: Unknown command");
				iPosition = _iLength;
			}			
			break;
		}
	}

	return iPosition;
}

// Not really used on GC-AM
bool CSIDevice_AMBaseboard::GetData(u32& _Hi, u32& _Low)
{
	_Low = 0;
	_Hi  = 0x00800000;

	return true;
}

void CSIDevice_AMBaseboard::SendCommand(u32 _Cmd, u8 _Poll)
{
	ERROR_LOG(SERIALINTERFACE, "unknown direct command     (0x%x)", _Cmd);
	PanicAlert("SI: (GCAM) Unknown direct command");
}
