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

#ifndef _EXIDEVICE_ETHERNET_H
#define _EXIDEVICE_ETHERNET_H

class CEXIETHERNET : public IEXIDevice
{
public:
	CEXIETHERNET();
	void SetCS(int _iCS);
	bool IsPresent();
	void Update();
	bool IsInterruptSet();
	void ImmWrite(u32 _uData,  u32 _uSize);
	u32  ImmRead(u32 _uSize);
	void DMAWrite(u32 _uAddr, u32 _uSize);
	void DMARead(u32 _uAddr, u32 _uSize);

private:
	// STATE_TO_SAVE
	u32 m_uPosition;
	u32 m_uCommand;
	bool mReadyToSend;
	unsigned int ID;
	u8 RegisterBlock[0x1000];

	void TransferByte(u8& _uByte);
};

// From Whinecube //

#define INVALID_P 0xFFFF

#define BBA_NCRA				0x00		/* Network Control Register A, RW */
#define BBA_NCRA_RESET			(1<<0)	/* RESET */
#define BBA_NCRA_ST0			(1<<1)	/* ST0, Start transmit command/status */
#define BBA_NCRA_ST1			(1<<2)	/* ST1,  " */
#define BBA_NCRA_SR				(1<<3)	/* SR, Start Receive */

#define BBA_NCRB				0x01		/* Network Control Register B, RW */
#define BBA_NCRB_PR				(1<<0)	/* PR, Promiscuous Mode */
#define BBA_NCRB_CA				(1<<1)	/* CA, Capture Effect Mode */
#define BBA_NCRB_PM				(1<<2)	/* PM, Pass Multicast */
#define BBA_NCRB_PB				(1<<3)	/* PB, Pass Bad Frame */
#define BBA_NCRB_AB				(1<<4)	/* AB, Accept Broadcast */
#define BBA_NCRB_HBD			(1<<5)	/* HBD, reserved */
#define BBA_NCRB_RXINTC0		(1<<6)	/* RXINTC, Receive Interrupt Counter */
#define BBA_NCRB_RXINTC1		(1<<7)	/*  " */
#define BBA_NCRB_1_PACKET_PER_INT  (0<<6)	/* 0 0 */
#define BBA_NCRB_2_PACKETS_PER_INT (1<<6)	/* 0 1 */
#define BBA_NCRB_4_PACKETS_PER_INT (2<<6)	/* 1 0 */
#define BBA_NCRB_8_PACKETS_PER_INT (3<<6)	/* 1 1 */

#define BBA_NWAYC 0x30		/* NWAY Configuration Register, RW, 84h */
#define   BBA_NWAYC_FD       (1<<0)	/* FD, Full Duplex Mode */
#define   BBA_NWAYC_PS100    (1<<1)	/* PS100/10, Port Select 100/10 */
#define   BBA_NWAYC_ANE      (1<<2)	/* ANE, Autonegotiation Enable */
#define   BBA_NWAYC_ANS_RA   (1<<3)	/* ANS, Restart Autonegotiation */
#define   BBA_NWAYC_LTE      (1<<7)	/* LTE, Link Test Enable */

#define BBA_NWAYS 0x31
#define   BBA_NWAYS_LS10	 (1<<0)
#define   BBA_NWAYS_LS100	 (1<<1)
#define   BBA_NWAYS_LPNWAY   (1<<2)
#define   BBA_NWAYS_ANCLPT	 (1<<3)
#define   BBA_NWAYS_100TXF	 (1<<4)
#define   BBA_NWAYS_100TXH	 (1<<5)
#define   BBA_NWAYS_10TXF	 (1<<6)
#define   BBA_NWAYS_10TXH	 (1<<7)

#endif
