// Copyright 2009 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <cstring>

#include "Common/CommonTypes.h"
#include "Common/MsgHandler.h"
#include "Common/Logging/Log.h"
#include "Core/HW/GCPad.h"
#include "Core/HW/SI_Device.h"
#include "Core/HW/SI_DeviceAMBaseboard.h"
#include "InputCommon/GCPadStatus.h"

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

	void Start(int node)
	{
		m_last_start = m_ptr;
		unsigned char hdr[3] = {0xe0, (unsigned char)node, 0};
		m_csum = 0;
		AddData(hdr, 3, 1);
	}
	void AddData(const void *data, size_t len)
	{
		AddData((const unsigned char*)data, len);
	}
	void AddData(const char *data)
	{
		AddData(data, strlen(data));
	}
	void AddData(int n)
	{
		unsigned char cs = n;
		AddData(&cs, 1);
	}

	void End()
	{
		int len = m_ptr - m_last_start;
		m_msg[m_last_start + 2] = len - 2; // assuming len <0xD0
		AddData(m_csum + len - 2);
	}

	void AddData(const unsigned char *dst, size_t len, int sync = 0)
	{
		while (len--)
		{
			int c = *dst++;
			if (!sync && ((c == 0xE0) || (c == 0xD0)))
			{
				m_msg[m_ptr++] = 0xD0;
				m_msg[m_ptr++] = c - 1;
			}
			else
			{
				m_msg[m_ptr++] = c;
			}

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
}

int CSIDevice_AMBaseboard::RunBuffer(u8* _pBuffer, int _iLength)
{
	// for debug logging only
	ISIDevice::RunBuffer(_pBuffer, _iLength);

	int iPosition = 0;
	while (iPosition < _iLength)
	{
		// read the command
		EBufferCommands command = static_cast<EBufferCommands>(_pBuffer[iPosition ^ 3]);
		iPosition++;

		// handle it
		switch (command)
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
							DEBUG_LOG(AMBASEBOARDDEBUG, "GC-AM: Command 10, %02x (READ STATUS&SWITCHES)", ptr(1));
							GCPadStatus PadStatus;
							memset(&PadStatus, 0 ,sizeof(PadStatus));
							Pad::GetStatus(ISIDevice::m_iDeviceNumber, &PadStatus);
							res[resp++] = 0x10;
							res[resp++] = 0x2;
							int d10_0 = 0xdf;

							if (PadStatus.triggerLeft)
								d10_0 &= ~0x80;
							if (PadStatus.triggerRight)
								d10_0 &= ~0x40;

							res[resp++] = d10_0;
							res[resp++] = d10_1;
							break;
						}
					case 0x12:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: Command 12, %02x %02x", ptr(1), ptr(2));
						res[resp++] = 0x12;
						res[resp++] = 0x00;
						break;
					case 0x11:
						{
							ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: Command 11, %02x (READ SERIAL NR)", ptr(1));
							char string[] = "AADE-01A14964511";
							res[resp++] = 0x11;
							res[resp++] = 0x10;
							memcpy(res + resp, string, 0x10);
							resp += 0x10;
							break;
						}
					case 0x15:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: CMD 15, %02x (READ FIRM VERSION)", ptr(1));
						res[resp++] = 0x15;
						res[resp++] = 0x02;
						res[resp++] = 0x00;
						res[resp++] = 0x29; // FIRM VERSION
						break;
					case 0x16:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: Command 16, %02x (READ FPGA VERSION)", ptr(1));
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
							ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: Command 1f, %02x %02x %02x %02x %02x (REGION)", ptr(1), ptr(2), ptr(3), ptr(4), ptr(5));
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
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: Command 31 (UNKNOWN)");
						res[resp++] = 0x31;
						res[resp++] = 0x02;
						res[resp++] = 0x00;
						res[resp++] = 0x00;
						break;
					case 0x32:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: Command 32 (UNKNOWN)");
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
							DEBUG_LOG(AMBASEBOARDDEBUG, "GC-AM: Command %02x, %02x %02x %02x %02x %02x %02x %02x (JVS IO)",
								ptr(0), ptr(1), ptr(2), ptr(3), ptr(4), ptr(5), ptr(6), ptr(7));
							int pptr = 2;
							JVSIOMessage msg;

							msg.Start(0);
							msg.AddData(1);

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
								DEBUG_LOG(AMBASEBOARDDEBUG, "JVS IO, node=%d, command=%02x", node, cmd);

								switch (cmd)
								{
								case 0x10: // get ID
									msg.AddData(1);
									{
										char buffer[12];
										sprintf(buffer, "JVS-node %02x", node);
										//msg.addData(buffer);
										msg.AddData("JAMMA I/O CONTROLLER");
									}
									msg.AddData(0);
									break;
								case 0x11: // cmd revision
									msg.AddData(1);
									msg.AddData(0x11);
									break;
								case 0x12: // jvs revision
									msg.AddData(1);
									msg.AddData(0x12);
									break;
								case 0x13: // com revision
									msg.AddData(1);
									msg.AddData(0x13);
									break;
								case 0x14: // get features
									msg.AddData(1);
									msg.AddData((void *)"\x01\x02\x0a\x00", 4);  // 2 player, 10 bit
									msg.AddData((void *)"\x02\x02\x00\x00", 4);  // 2 coin slots
									//msg.addData((void *)"\x03\x02\x08\x00", 4);
									msg.AddData((void *)"\x00\x00\x00\x00", 4);
									break;
								case 0x15:
									while (*jvs_io++) {};
									msg.AddData(1);
									break;
								case 0x20:
									{
										int nr_players = *jvs_io++;
										int bytes_per_player = *jvs_io++; /* ??? */
										int j;
										msg.AddData(1);

										msg.AddData(0); // tilt
										for (i=0; i<nr_players; ++i)
										{
											GCPadStatus PadStatus;
											Pad::GetStatus(i, &PadStatus);
											unsigned char player_data[2] = {0,0};
											if (PadStatus.button & PAD_BUTTON_START)
												player_data[0] |= 0x80;
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
											if (PadStatus.button & PAD_BUTTON_B)
												player_data[0] |= 0x01;

											if (PadStatus.button & PAD_BUTTON_X)
												player_data[1] |= 0x80;
											if (PadStatus.button & PAD_BUTTON_Y)
												player_data[1] |= 0x40;
											if (PadStatus.button & PAD_TRIGGER_L)
												player_data[1] |= 0x20;
											if (PadStatus.button & PAD_TRIGGER_R)
												player_data[1] |= 0x10;

											for (j=0; j<bytes_per_player; ++j)
												msg.AddData(player_data[j&1]);
										}
										break;
									}
								case 0x21: // coin
									{
										int slots = *jvs_io++;
										msg.AddData(1);
										GCPadStatus PadStatus;
										Pad::GetStatus(0, &PadStatus);
										while (slots--)
										{
											msg.AddData(0);
											msg.AddData((PadStatus.button & PAD_BUTTON_START) ? 1 : 0);
										}
										break;
									}
								case 0x22: // analog
									{
										break;
									}
								case 0xf0:
									if (*jvs_io++ == 0xD9)
										ERROR_LOG(AMBASEBOARDDEBUG, "JVS RESET");
									msg.AddData(1);

									d10_1 |= 1;
									break;
								case 0xf1:
									node = *jvs_io++;
									ERROR_LOG(AMBASEBOARDDEBUG, "JVS SET ADDRESS, node=%d", node);
									msg.AddData(node == 1);
									break;
								default:
									break;
								}

								pptr += jvs_io_length;

							}

							msg.End();

							res[resp++] = ptr(0);

							unsigned char *buf = msg.m_msg;
							int len = msg.m_ptr;
							res[resp++] = len;
							for (i=0; i<len; ++i)
								res[resp++] = buf[i];
							break;
						}
					case 0x60:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: Command 60, %02x %02x %02x", ptr(1), ptr(2), ptr(3));
						break;
					default:
						ERROR_LOG(AMBASEBOARDDEBUG, "GC-AM: Command %02x (unknown) %02x %02x %02x %02x %02x", ptr(0), ptr(1), ptr(2), ptr(3), ptr(4), ptr(5));
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
				DEBUG_LOG(AMBASEBOARDDEBUG, "Command send back: %s", logptr);
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
				ERROR_LOG(SERIALINTERFACE, "Unknown SI command     (0x%x)", command);
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
	ERROR_LOG(SERIALINTERFACE, "Unknown direct command     (0x%x)", _Cmd);
	PanicAlert("SI: (GCAM) Unknown direct command");
}
