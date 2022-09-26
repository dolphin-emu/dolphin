// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#include "Core/HW/SI/SI.h"
#include "Core/HW/SI/SI_Device.h"
#include "Core/HW/SI/SI_DeviceAMBaseboard.h"

#include "Common/Common.h" // Common
#include "Common/ChunkFile.h"
#include "Common/IOFile.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "InputCommon/GCPadStatus.h"
#include "Core/HW/GCPad.h"
#include "Core/ConfigManager.h"
#include "Core/HW/AMBaseboard.h"

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
			}
			else
			{
				m_msg[m_ptr++] = c;
			}

			if (!sync)
				m_csum += c;
			sync = 0;
			if (m_ptr >= 0x80)
				PanicAlertFmtT("JVSIOMessage overrun!");
		}
	}
}; // end class JVSIOMessage


// AM-Baseboard device on SI
CSIDevice_AMBaseboard::CSIDevice_AMBaseboard(SerialInterface::SIDevices device, int _iDeviceNumber)
	: SerialInterface::ISIDevice(device, _iDeviceNumber)
{
	memset(m_coin, 0, sizeof(m_coin));

	m_card_memory_size = 0;
	m_card_is_inserted = 0;

	m_card_offset = 0;
	m_card_command = 0;
	m_card_clean = 0;

	m_card_write_length = 0;
	m_card_wrote = 0;

	m_card_read_length = 0;
	m_card_read = 0;

	m_card_bit = 0;
	m_card_state_call_count = 0;

	m_wheelinit		= 0;

	m_motorinit		= 0;
	m_motorforce	= 0;
	m_motorforce_x	= 0;
	m_motorforce_y	= 0;

	memset( m_motorreply, 0, sizeof(m_motorreply) );
}
int CSIDevice_AMBaseboard::RunBuffer(u8* _pBuffer, int _iLength)
{
	// for debug logging only
	ISIDevice::RunBuffer(_pBuffer, _iLength);

	int iPosition = 0;
	while(iPosition < _iLength)
	{
		// read the command
		EBufferCommands command = static_cast<EBufferCommands>(_pBuffer[iPosition ^ 3]);
		iPosition++;

		// handle it
		switch(command)
		{
		case CMD_RESET: // returns ID and dip switches
			{
				*(u32*)&_pBuffer[0] = SerialInterface::SI_AM_BASEBOARD|0x100; // 0x100 is progressive flag according to dip switch
				iPosition = _iLength; // break the while loop
			}
			break;
		case CMD_GCAM:
			{
				// calculate checksum over buffer
				int csum = 0;
				for( int i=0; i <_iLength; ++i )
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
						DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 10, {:02x} (READ STATUS&SWITCHES)", ptr(1));
						SPADStatus PadStatus;
						memset(&PadStatus, 0 ,sizeof(PadStatus));
						Pad::GetStatus(ISIDevice::GetDeviceNumber, &PadStatus);
						res[resp++] = 0x10;
						res[resp++] = 0x2;
						int d10_0 = 0xFF;

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
						NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 11, {:02x} (READ SERIAL NR)", ptr(1));
						char string[] = "AADE-01A14964511";
						res[resp++] = 0x11;
						res[resp++] = 0x10;
						memcpy(res + resp, string, 0x10);
						resp += 0x10;
						break;
					}
					case 0x12:
						NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 12, {:02x} {:02x}", ptr(1), ptr(2));
						res[resp++] = 0x12;
						res[resp++] = 0x00;
						break;
					case 0x15:
						NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 15, {:02x} (READ FIRM VERSION)", ptr(1));
						res[resp++] = 0x15;
						res[resp++] = 0x02;
						 // FIRM VERSION
						res[resp++] = 0x00;
						res[resp++] = 0x44;
						break;
					case 0x16:
						NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 16, {:02x} (READ FPGA VERSION)", ptr(1));
						res[resp++] = 0x16;
						res[resp++] = 0x02;
						 // FPGAVERSION
						res[resp++] = 0x07;
						res[resp++] = 0x09;
						break;
					case 0x1f:
					{
						NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 1f, {:02x} {:02x} {:02x} {:02x} {:02x} (REGION)", ptr(1), ptr(2), ptr(3), ptr(4), ptr(5));
						unsigned char string[] =
							"\x00\x00\x30\x00"
							"\x01\xfe\x00\x00" // JAPAN
							//"\x02\xfd\x00\x00" // USA
							//"\x03\xfc\x00\x00" // export
							"\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff";
						res[resp++] = 0x1f;
						res[resp++] = 0x14;

						for( int i=0; i<0x14; ++i )
							res[resp++] = string[i];
						break;
					}
					case 0x23:
						if( ptr(1) )
						{
							DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 23, {:02x} {:02x} {:02x} {:02x} {:02x}", ptr(1), ptr(2), ptr(3), ptr(4), ptr(5) );
							res[resp++] = 0x23;
							res[resp++] = 0x00;
						}
						else
						{
							res[resp++] = 0x23;
							res[resp++] = 0x00;
						}
						break;
					case 0x24:
						if( ptr(1) )
						{
							DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 0x24, {:02x} {:02x} {:02x} {:02x} {:02x}", ptr(1), ptr(2), ptr(3), ptr(4), ptr(5) );
							res[resp++] = 0x24;
							res[resp++] = 0x00;
						}
						else
						{
							res[resp++] = 0x24;
							res[resp++] = 0x00;
						}
						break;
					case 0x31:
					//	NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 31 (MOTOR) {:02x} {:02x} {:02x} {:02x} {:02x}", ptr(1), ptr(2), ptr(3), ptr(4), ptr(5));

						// Command Length
						if( ptr(1) )
						{
							// All commands are OR'd with 0x80
							// Last byte (ptr(5)) is checksum which we don't care about
							u32 cmd =(ptr(2)^0x80) << 16;
								cmd|= ptr(3) << 8;
								cmd|= ptr(4);

							NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 31 (SERIAL) Command:{:06x}", cmd );

							// Serial - Wheel
							if( cmd == 0x7FFFF0 )
							{
								res[resp++] = 0x31;
								res[resp++] = 0x03;

								res[resp++] = 'C';
								res[resp++] = '0';

								if( m_wheelinit == 0 )
								{
									res[resp++] = '1';
									m_wheelinit = 1;
								} else {
									res[resp++] = '6';
								}
								break;
							}
/*
							// Serial - Motor
							m_motorreply[0] = 0x31;
							m_motorreply[1] = 0x04;

							// Status
							m_motorreply[2] = 0;
							m_motorreply[3] = 0;
							// error
							m_motorreply[4] = 0;

							switch(cmd>>16)
							{
								case 0x00:
									if( cmd == 0 )
										m_motorforce = 0;
									break;
								case 0x04:
									m_motorforce	= 1;
									m_motorforce_x	= (s16)(cmd<<4);
									m_motorforce_y  = m_motorforce_x;
									break;
								case 0x70:
									m_motorforce = 0;
									break;
								case 0x7A:
									m_motorforce	= 1;
									m_motorforce_x	= (s16)(cmd & 0xFFFF);
									m_motorforce_y  = m_motorforce_x;
									break;
								default:
									break;
							}

							//Checksum
							m_motorreply[5] = m_motorreply[2] ^ m_motorreply[3] ^ m_motorreply[4];
							resp += 6;

						} else {
							if( m_motorreply[0] )
							{
								memcpy( res+resp, m_motorreply, sizeof(m_motorreply) );
								resp += sizeof(m_motorreply);
							} else {
								res[resp++] = 0x31;
								res[resp++] = 0x00;
							}
						*/
						}
						break;
					case 0x32:
					//	NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 32 (CARD-Interface)");
						if( ptr(1) )
						{
							if( ptr(1) == 1 && ptr(2) == 0x05 )
							{
								if( m_card_read_length )
								{
									res[resp++] = 0x32;
									u32 ReadLength = m_card_read_length - m_card_read;

									if( ReadLength > 0x2F )
										ReadLength = 0x2F;

									res[resp++] = ReadLength;	// 0x2F (max size per packet)

									memcpy( res+resp, m_card_read_packet+m_card_read, ReadLength );

									resp			+= ReadLength;
									m_card_read	+= ReadLength;

									if( m_card_read >= m_card_read_length )
										m_card_read_length = 0;

									break;
								}

								res[resp++] = 0x32;
								u32 CMDLenO	= resp;
								res[resp++] = 0x00;	// len

								res[resp++] = 0x02; //
								u32 ChkStart = resp;

								res[resp++] = 0x00;	// 0x00		 len

								switch(m_card_command)
								{
								case CARD_INIT:
									res[resp++] = 0x10;	// 0x01
									res[resp++] = 0x00;	// 0x02
									res[resp++] = '0';	// 0x03
									break;
								case CARD_IS_PRESENT:
									res[resp++] = 0x40;	// 0x01
									res[resp++] = 0x22;	// 0x02
									res[resp++] = '0';	// 0x03
									break;
								case CARD_GET_CARD_STATE:
									res[resp++] = 0x20;	// 0x01
									res[resp++] = 0x20|m_card_bit;	// 0x02
									/*
										bit 0: PLease take your card
										bit 1: endless waiting casues UNK_E to be called
									*/
									res[resp++] = 0x00;	// 0x03
									break;
								case CARD_7A:
									res[resp++] = 0x7A;	// 0x01
									res[resp++] = 0x00;	// 0x02
									res[resp++] = 0x00;	// 0x03
									break;
								case CARD_78:
									res[resp++] = 0x78;	// 0x01
									res[resp++] = 0x00;	// 0x02
									res[resp++] = 0x00;	// 0x03
									break;
								case CARD_WRITE_INFO:
									res[resp++] = 0x7C;	// 0x01
									res[resp++] = 0x02;	// 0x02
									res[resp++] = 0x00;	// 0x03
									break;
								case CARD_D0:
									res[resp++] = 0xD0;	// 0x01
									res[resp++] = 0x00;	// 0x02
									res[resp++] = 0x00;	// 0x03
									break;
								case CARD_80:
									res[resp++] = 0x80;	// 0x01
									res[resp++] = 0x01;	// 0x02
									res[resp++] = '0';	// 0x03
									break;
								case CARD_CLEAN_CARD:
									res[resp++] = 0xA0;	// 0x01
									res[resp++] = 0x02;	// 0x02
									res[resp++] = 0x00;	// 0x03
									break;
								case CARD_LOAD_CARD:
									res[resp++] = 0xB0;	// 0x01
									res[resp++] = 0x02;	// 0x02
									res[resp++] = '0';	// 0x03
									break;
								case CARD_WRITE:
									res[resp++] = 'S';	// 0x01
									res[resp++] = 0x02;	// 0x02
									res[resp++] = 0x00;	// 0x03
									break;
								case CARD_READ:
									res[resp++] = 0x33;	// 0x01
									res[resp++] = 0x02;	// 0x02
									res[resp++] = 'S';	// 0x03
									break;
								}

								res[resp++] = '0';	// 0x04
								res[resp++] = 0x00;	// 0x05

								res[resp++] = 0x03;	// 0x06

								res[ChkStart] = resp-ChkStart;	// 0x00 len

								u32 i;
								res[resp] = 0;		// 0x07
								for( i=0; i < res[ChkStart]; ++i )
									res[resp] ^= res[ChkStart+i];

								resp++;

								res[CMDLenO] = res[ChkStart] + 2;

							} else {

								for( u32 i=0; i < ptr(1); ++i )
									m_card_buffer[m_card_offset+i] = ptr(2+i);

								m_card_offset += ptr(1);

								//Check if we got complete CMD

								if( m_card_buffer[0] == 0x02 )
								if( m_card_buffer[1] == m_card_offset - 2 )
								{
									if( m_card_buffer[m_card_offset-2] == 0x03 )
									{
										u32 cmd = m_card_buffer[2] << 24;
											cmd|= m_card_buffer[3] << 16;
											cmd|= m_card_buffer[4] <<  8;
											cmd|= m_card_buffer[5] <<  0;

										switch(cmd)
										{
										case 0x10000000:
											NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD Init");
											m_card_command = CARD_INIT;

											m_card_write_length		= 0;
											m_card_bit				= 0;
											m_card_memory_size			= 0;
											m_card_state_call_count	= 0;
											break;
										case 0x20000000:
										{
											NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD GetState({:02x})", m_card_bit );
											m_card_command = CARD_GET_CARD_STATE;

											if( m_card_memory_size )
											{
												m_card_state_call_count++;
												if( m_card_state_call_count > 10 )
												{
													if( m_card_bit & 2 )
														m_card_bit &= ~2;
													else
														m_card_bit |= 2;

													m_card_state_call_count = 0;
												}
											}

											if( m_card_clean == 1 )
											{
												m_card_clean = 2;
											} else if( m_card_clean == 2 )
											{
												std::string card_filename( File::GetUserPath(D_TRIUSER_IDX) +
												"tricard_" + SConfig::GetInstance().GetGameID() + ".bin" );

												if( File::Exists( card_filename ) )
												{
													m_card_memory_size = (u32)File::GetSize(card_filename);
													if( m_card_memory_size )
														m_card_bit = 2;
												}
												m_card_clean = 0;
											}
											break;
										}
										case 0x40000000:
											NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD IsPresent");
											m_card_command = CARD_IS_PRESENT;
											break;
										case 0x7A000000:
											NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD Unknown7A");
											m_card_command = CARD_7A;
											break;
										case 0xB0000000:
											NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD LoadCard");
											m_card_command = CARD_LOAD_CARD;
											break;
										case 0xA0000000:
											NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD IsCleanCard");
											m_card_command = CARD_CLEAN_CARD;
											m_card_clean = 1;
											break;
										case 0x33000000:
										{
											NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD Read");
											m_card_command = CARD_READ;

											//Prepare read packet
											memset( m_card_read_packet, 0, 0xDB );
											u32 POff=0;

											std::string card_filename( File::GetUserPath(D_TRIUSER_IDX) +
											"tricard_" + SConfig::GetInstance().GetGameID() + ".bin" );

											if( File::Exists( card_filename ) )
											{
												File::IOFile card = File::IOFile( card_filename, "rb+" );
												if( m_card_memory_size == 0 )
												{
													m_card_memory_size = (u32)card.GetSize();
												}

												card.ReadBytes( m_card_memory, m_card_memory_size );
												card.Close();

												m_card_is_inserted = 1;
											}

											m_card_read_packet[POff++] = 0x02;	// SUB CMD
											m_card_read_packet[POff++] = 0x00;	// SUB CMDLen

											m_card_read_packet[POff++] = 0x33;	// CARD CMD

											if( m_card_is_inserted )
											{
												m_card_read_packet[POff++] = '1';	// CARD Status
											} else {
												m_card_read_packet[POff++] = '0';	// CARD Status
											}

											m_card_read_packet[POff++] = '0';		//
											m_card_read_packet[POff++] = '0';		//


											//Data reply
											memcpy( m_card_read_packet + POff, m_card_memory, m_card_memory_size );
											POff += m_card_memory_size;

											m_card_read_packet[POff++] = 0x03;

											m_card_read_packet[1] = POff-1;	// SUB CMDLen

											u32 i;
											for( i=0; i < POff-1; ++i )
												m_card_read_packet[POff] ^= m_card_read_packet[1+i];

											POff++;

											m_card_read_length	= POff;
											m_card_read	= 0;
											break;
										}
										case 0x53000000:
										{
											m_card_command = CARD_WRITE;

											m_card_memory_size = m_card_buffer[1] - 9;

											memcpy( m_card_memory, m_card_buffer+9, m_card_memory_size );

											NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "CARDWrite: {:u}", m_card_memory_size );

											std::string card_filename( File::GetUserPath(D_TRIUSER_IDX) +
											"tricard_" + SConfig::GetInstance().GetGameID() + ".bin" );

											File::IOFile card = File::IOFile( card_filename, "wb+" );
											card.WriteBytes( m_card_memory, m_card_memory_size );
											card.Close();

											m_card_bit = 2;

											m_card_state_call_count = 0;
											break;
										}
										case 0x78000000:
											DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD Unknown78");
											m_card_command	= CARD_78;
											break;
										case 0x7C000000:
											DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD WriteCardInfo");
											m_card_command	= CARD_WRITE_INFO;
											break;
										case 0x7D000000:
											DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD Print");
											m_card_command	= CARD_7D;
											break;
										case 0x80000000:
											DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD Unknown80");
											m_card_command	= CARD_80;
											break;
										case 0xD0000000:
											DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command CARD UnknownD0");
											m_card_command	= CARD_D0;
											break;
										default:
										//	NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "CARD:Unhandled cmd!");
										//	NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "CARD:[{:08x}]", cmd );
										//	hexdump( m_card_buffer, m_card_offset );
											break;
										}
										m_card_offset = 0;
									}
								}

								res[resp++] = 0x32;
								res[resp++] = 0x01;	// len
								res[resp++] = 0x06;	// OK
							}
						} else {
							res[resp++] = 0x32;
							res[resp++] = 0x00;	// len
						}
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
							DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command {:02x}, {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} {:02x} (JVS IO)",
								ptr(0), ptr(1), ptr(2), ptr(3), ptr(4), ptr(5), ptr(6), ptr(7));
							int pptr = 2;
							JVSIOMessage msg;

							msg.start(0);
							msg.addData(1);

							unsigned char jvs_io_buffer[0x80];
							int nr_bytes = ptr(pptr + 2); // byte after e0 xx
							int jvs_io_length = 0;

							for( int i=0; i<nr_bytes + 3; ++i )
								jvs_io_buffer[jvs_io_length++] = ptr(pptr + i);

							int node = jvs_io_buffer[1];

							unsigned char *jvs_io = jvs_io_buffer + 3;
							jvs_io_length--; // checksum

							while (jvs_io < (jvs_io_buffer + jvs_io_length))
							{
								int cmd = *jvs_io++;
								DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "JVS IO, node={:d}, command={:02x}", node, cmd);

								switch (cmd)
								{
								// read ID data
								case 0x10:
									msg.addData(1);
									if (!memcmp(SConfig::GetInstance().GetGameID().c_str(), "RELSAB", 6))
										msg.addData("namco ltd.;FCA-1;Ver1.01;JPN,Multipurpose + Rotary Encoder");
									else
										msg.addData("SEGA ENTERPRISES,LTD.;I/O BD JVS;837-13551;Ver1.00");
									msg.addData(0);
									break;
								// get command format revision
								case 0x11:
									msg.addData(1);
									msg.addData(0x11);
									break;
								// get JVS revision
								case 0x12:
									msg.addData(1);
									msg.addData(0x20);
									break;
								// get supported communications versions
								case 0x13:
									msg.addData(1);
									msg.addData(0x10);
									break;

								// get slave features
								/*
									0x01: Player count, Bit per channel
									0x02: Coin slots
									0x03: Analog-in
									0x04: Rotary
									0x05: Keycode
									0x06: Screen, x, y, ch
									....: unused
									0x10: Card
									0x11: Hopper-out
									0x12: Driver-out
									0x13: Analog-out
									0x14: Character, Line (?)
									0x15: Backup
								*/
								case 0x14:
									msg.addData(1);
									switch(AMBaseboard::GetControllerType())
									{
										case 1:
											// 1 Player (12-bits), 1 Coin slot, 8 Analog-in
											msg.addData((void *)"\x01\x01\x0C\x00", 4);
											msg.addData((void *)"\x02\x01\x00\x00", 4);
											msg.addData((void *)"\x03\x06\x00\x00", 4);
											msg.addData((void *)"\x00\x00\x00\x00", 4);
											break;
										case 2:
											// 2 Player, 2 Coin slots, 4 Analogs, 8Bit out
											msg.addData((void *)"\x01\x02\x0D\x00", 4);
											msg.addData((void *)"\x02\x02\x00\x00", 4);
											msg.addData((void *)"\x03\x04\x00\x00", 4);
											msg.addData((void *)"\x12\x08\x00\x00", 4);
											msg.addData((void *)"\x00\x00\x00\x00", 4);
											break;
										default:
										case 3:
											// 1 Player, 1 Coin slot, 3 Analogs, 8Bit out
											msg.addData((void *)"\x01\x01\x13\x00", 4);
											msg.addData((void *)"\x02\x02\x00\x00", 4);
											msg.addData((void *)"\x03\x03\x00\x00", 4);
											msg.addData((void *)"\x13\x08\x00\x00", 4);
											msg.addData((void *)"\x00\x00\x00\x00", 4);
											break;
									}
									break;
								// convey ID of main board
								case 0x15:
									while (*jvs_io++) {};
									msg.addData(1);
									break;
								// read switch inputs
								case 0x20:
								{
									int player_count		= *jvs_io++;
									int player_byte_count	= *jvs_io++;

									msg.addData(1);

									SPADStatus PadStatus;
									Pad::GetStatus(0, &PadStatus);

									// Test button
									if( PadStatus.button & PAD_BUTTON_Y )
										msg.addData(0x80);
									else
										msg.addData(0x00);

									for( int i=0; i<player_count; ++i )
									{
										SPADStatus PadStatus;
										Pad::GetStatus(i, &PadStatus);
										unsigned char player_data[3] = {0,0,0};

										switch(AMBaseboard::GetControllerType())
										{
										// Controller configuration for F-Zero AX
										case 1:
											// Start
											if( PadStatus.button & PAD_BUTTON_START )
												player_data[0] |= 0x80;
											// Service button
											if( PadStatus.button & PAD_BUTTON_X )
												player_data[0] |= 0x40;
											// Boost
											if( PadStatus.button & PAD_BUTTON_A )
												player_data[0] |= 0x02;
											// View Change 1
											if( PadStatus.button & PAD_BUTTON_RIGHT )
												player_data[0] |= 0x20;
											// View Change 2
											if( PadStatus.button & PAD_BUTTON_LEFT )
												player_data[0] |= 0x10;
											// View Change 3
											if( PadStatus.button & PAD_BUTTON_UP )
												player_data[0] |= 0x08;
											// View Change 4
											if( PadStatus.button & PAD_BUTTON_DOWN )
												player_data[0] |= 0x04;
											break;
										// Controller configuration for Virtua Striker games
										case 2:
											// Start
											if( PadStatus.button & PAD_BUTTON_START )
												player_data[0] |= 0x80;
											// Service button
											if( PadStatus.button & PAD_BUTTON_X )
												player_data[0] |= 0x40;
											//  Pass
											if( PadStatus.button & PAD_TRIGGER_L )
												player_data[0] |= 0x01;
											//  Pass
											if( PadStatus.button & PAD_TRIGGER_R )
												player_data[0] |= 0x02;
											// Shoot
											if( PadStatus.button & PAD_BUTTON_A )
												player_data[1] |= 0x80;
											// Dash
											if( PadStatus.button & PAD_BUTTON_B )
												player_data[1] |= 0x40;
											// Tactics (U)
											if( PadStatus.button & PAD_BUTTON_LEFT )
												player_data[0] |= 0x20;
											// Tactics (M)
											if( PadStatus.button & PAD_BUTTON_UP )
												player_data[0] |= 0x08;
											// Tactics (D)
											if( PadStatus.button & PAD_BUTTON_RIGHT )
												player_data[0] |= 0x04;
											break;
										// Controller configuration for Mario Kart and other games
										default:
										case 3:
											// Start
											if( PadStatus.button & PAD_BUTTON_START )
												player_data[0] |= 0x80;
											// Service button
											if( PadStatus.button & PAD_BUTTON_X )
												player_data[0] |= 0x40;
											// Item button
											if( PadStatus.button & PAD_BUTTON_A )
												player_data[1] |= 0x20;
											// VS-Cancel button
											if( PadStatus.button & PAD_BUTTON_B )
												player_data[1] |= 0x02;
											break;
										}

										for( int j=0; j<player_byte_count; ++j )
											msg.addData(player_data[j]);
									}
									break;
								}
								// read m_coin inputs
								case 0x21:
								{
									SPADStatus PadStatus;
									int slots = *jvs_io++;
									msg.addData(1);
									for( int i = 0; i < slots; i++ )
									{
										Pad::GetStatus(i, &PadStatus);
										if ((PadStatus.button & PAD_TRIGGER_Z) && !m_coin_pressed[i])
										{
											m_coin[i]++;
										}
										m_coin_pressed[i]=PadStatus.button & PAD_TRIGGER_Z;
										msg.addData((m_coin[i]>>8)&0x3f);
										msg.addData(m_coin[i]&0xff);
									}
									//NOTICE_LOG_FMT(AMBASEBOARDDEBUG, "JVS-IO:Get Coins Slots:%u Unk:%u", slots, unk );
									break;
								}
								//  read analog inputs
								case 0x22:
								{
									msg.addData(1);	// status
									int analogs = *jvs_io++;
									SPADStatus PadStatus;
									Pad::GetStatus(0, &PadStatus);

									switch(AMBaseboard::GetControllerType())
									{
										// F-Zero AX
										case 1:
											// Steering
											if( m_motorforce )
											{
												msg.addData( m_motorforce_x >> 8 );
												msg.addData( m_motorforce_x & 0xFF );

												msg.addData( m_motorforce_y >> 8);
												msg.addData( m_motorforce_y & 0xFF );
											}
											else
											{
												msg.addData(PadStatus.stickX);
												msg.addData((u8)0);

												msg.addData(PadStatus.stickY);
												msg.addData((u8)0);
											}

											// Unused
											msg.addData((u8)0);
											msg.addData((u8)0);
											msg.addData((u8)0);
											msg.addData((u8)0);

											// Gas
											msg.addData(PadStatus.triggerRight);
											msg.addData((u8)0);

											// Brake
											msg.addData(PadStatus.triggerLeft);
											msg.addData((u8)0);

											break;
										//  Virtua Strike games
										case 2:
											SPADStatus PadStatus2;
											Pad::GetStatus(1, &PadStatus2);

											msg.addData(PadStatus.stickX);
											msg.addData((u8)0);
											msg.addData(PadStatus.stickY);
											msg.addData((u8)0);

											msg.addData(PadStatus2.stickX);
											msg.addData((u8)0);
											msg.addData(PadStatus2.stickY);
											msg.addData((u8)0);
											break;
										// Mario Kart and other games
										case 3:
											// Steering
											msg.addData(PadStatus.stickX);
											msg.addData((u8)0);

											// Gas
											msg.addData(PadStatus.triggerRight);
											msg.addData((u8)0);

											// Brake
											msg.addData(PadStatus.triggerLeft);
											msg.addData((u8)0);
											break;
									}
								}
								// decrease number of coins
								case 0x30:
								{
									int slot = *jvs_io++;
									m_coin[slot]-= (*jvs_io++<<8)|*jvs_io++;
									msg.addData(1);
									break;
								}
								// general-purpose output
								case 0x32:
								{
									int bytes = *jvs_io++;
									while (bytes--) {*jvs_io++;}
									msg.addData(1);
									break;
								}
								// output the total number of coins
								case 0x35:
								{
									int slot = *jvs_io++;
									m_coin[slot]+= (*jvs_io++<<8)|*jvs_io++;
									msg.addData(1);
									break;
								}
								case 0x70: // custom namco's command subset
								{
									int cmd = *jvs_io++;
									if (cmd == 0x18) { // id check
										jvs_io+=4;
										msg.addData(1);
										msg.addData(0xff);
									} else {
										msg.addData(1);
										///ERROR_LOG_FMT(AMBASEBOARDDEBUG, "JVS-IO:Unknown");
									}
									break;
								}
								case 0xf0:
									if (*jvs_io++ == 0xD9)
										ERROR_LOG_FMT(AMBASEBOARDDEBUG, "JVS RESET");
									msg.addData(1);

									d10_1 |= 1;
									break;
								case 0xf1:
									node = *jvs_io++;
									ERROR_LOG_FMT(AMBASEBOARDDEBUG, "JVS SET ADDRESS, node={:d}", node);
									msg.addData(node == 1);
									d10_1 &= ~1;
									break;
								default:
									ERROR_LOG_FMT(AMBASEBOARDDEBUG, "JVS IO, node={:d}, command={:02x}", node, cmd);
									break;
								}

								pptr += jvs_io_length;
							}

							msg.end();

							res[resp++] = ptr(0);

							unsigned char *buf = msg.m_msg;
							int len = msg.m_ptr;
							res[resp++] = len;

							for( int i=0; i<len; ++i )
								res[resp++] = buf[i];
							break;
						}
					case 0x60:
						ERROR_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command 60, {:02x} {:02x} {:02x}", ptr(1), ptr(2), ptr(3));
						break;
					default:
						ERROR_LOG_FMT(AMBASEBOARDDEBUG, "GC-AM: Command {:02x} (unknown) {:02x} {:02x} {:02x} {:02x} {:02x}", ptr(0), ptr(1), ptr(2), ptr(3), ptr(4), ptr(5));
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

				for( int i=0; i<0x7F; ++i )
				{
					csum += ptr(i) = res[i];
					log += sprintf(log, "{:02x} ", ptr(i));
				}
				ptr(0x7f) = ~csum;
				DEBUG_LOG_FMT(AMBASEBOARDDEBUG, "Command send back: {:s}", logptr);
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
				ERROR_LOG_FMT(SERIALINTERFACE, "Unknown SI command     (0x{:x})", command);
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
	ERROR_LOG_FMT(SERIALINTERFACE, "Unknown direct command     (0x{:x})", _Cmd);
	PanicAlertFmtT("SI: (GCAM) Unknown direct command");
}
