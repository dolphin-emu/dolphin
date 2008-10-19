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
#include "../Plugins/Plugin_Wiimote.h"

#include "WII_IPC_HLE_WiiMote.h"
#include "l2cap.h"
#include "WiiMote_HID_Attr.h"

#if defined(_MSC_VER)
#pragma pack(push, 1)
#endif


#define HIDP_OUTPUT_CHANNEL			0x11
#define HIDP_INPUT_CHANNEL			0x13

// #define HID_OUTPUT_SCID 0x1234
// #define HID_INPUT_SCID 0x5678


#define HID_OUTPUT_SCID 0x0040
#define HID_INPUT_SCID 0x0040

struct SL2CAP_Header
{
	u16 Length;
	u16 CID;
};

/* L2CAP command codes */
#define L2CAP_COMMAND_REJ 0x01
#define L2CAP_CONN_REQ    0x02
#define L2CAP_CONN_RSP    0x03
#define L2CAP_CONF_REQ    0x04
#define L2CAP_CONF_RSP    0x05
#define L2CAP_DISCONN_REQ 0x06
#define L2CAP_DISCONN_RSP 0x07
#define L2CAP_ECHO_REQ    0x08
#define L2CAP_ECHO_RSP    0x09
#define L2CAP_INFO_REQ    0x0a
#define L2CAP_INFO_RSP    0x0b

/* connect result */
#define L2CAP_CR_SUCCESS    0x0000
#define L2CAP_CR_PEND       0x0001
#define L2CAP_CR_BAD_PSM    0x0002
#define L2CAP_CR_SEC_BLOCK  0x0003
#define L2CAP_CR_NO_MEM     0x0004

/* connect status */
#define L2CAP_CS_NO_INFO      0x0000
#define L2CAP_CS_AUTHEN_PEND  0x0001
#define L2CAP_CS_AUTHOR_PEND  0x0002


struct SL2CAP_Command
{
	u8 code;
	u8 ident;
	u16 len;
};

struct SL2CAP_CommandConnectionReq // 0x02
{
	u16 psm;
	u16 scid;
};

struct SL2CAP_ConnectionResponse // 0x03
{
	u16 dcid;
	u16 scid;
	u16 result;
	u16 status;
};

struct SL2CAP_Options
{
	u8 type;
	u8 length;
};

struct SL2CAP_OptionsMTU
{
	u16 MTU;
};

struct SL2CAP_OptionsFlushTimeOut
{
	u16 TimeOut;
};

struct SL2CAP_CommandConfigurationReq // 0x04
{
	u16 dcid;
	u16 flags;
};

struct SL2CAP_CommandConfigurationResponse // 0x05
{
	u16 scid;
	u16 flags;
	u16 result;
};

struct SL2CAP_CommandDisconnectionReq // 0x06
{
	u16 dcid;
	u16 scid;
};

struct SL2CAP_CommandDisconnectionResponse // 0x07
{
	u16 dcid;
	u16 scid;
};

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

static CWII_IPC_HLE_Device_usb_oh1_57e_305* s_Usb;

namespace Core {
	void Callback_WiimoteInput(const void* _pData, u32 _Size) {
		s_Usb->m_WiiMotes[0].SendL2capData(HID_OUTPUT_SCID, _pData, _Size);
	}
}

CWII_IPC_HLE_WiiMote::CWII_IPC_HLE_WiiMote(CWII_IPC_HLE_Device_usb_oh1_57e_305* _pHost, int _Number)
	: m_Name("Nintendo RVL-CNT-01")
	, m_pHost(_pHost)
{
	s_Usb = _pHost;
	LOG(WIIMOTE, "Wiimote %i constructed", _Number);

	m_BD.b[0] = 0x11;
	m_BD.b[1] = 0x02;
	m_BD.b[2] = 0x19;
	m_BD.b[3] = 0x79;
	m_BD.b[4] = 0x00;
	m_BD.b[5] = _Number;

	m_ControllerConnectionHandle = 0x100 + _Number;

	uclass[0]= 0x00;
	uclass[1]= 0x04;
	uclass[2]= 0x48;

	features[0] = 0xBC;
	features[1] = 0x02;
	features[2] = 0x04;
	features[3] = 0x38;
	features[4] = 0x08;
	features[5] = 0x00;
	features[6] = 0x00;
	features[7] = 0x00;

	lmp_version = 0x2;
	lmp_subversion = 0x229;
}

void CWII_IPC_HLE_WiiMote::SendACLFrame(u8* _pData, u32 _Size)
{
	// dump raw data
	{
		LOG(WIIMOTE, "SendToDevice: 0x%x", GetConnectionHandle());
		std::string Temp;
		for (u32 j=0; j<_Size; j++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", _pData[j]);
			Temp.append(Buffer);
		}
		LOG(WIIMOTE, "   Data: %s", Temp.c_str());
	}

	// parse the command
	SL2CAP_Header* pHeader = (SL2CAP_Header*)_pData;
	u8* pData = _pData + sizeof(SL2CAP_Header);
	u32 DataSize = _Size - sizeof(SL2CAP_Header);

	LOG(WIIMOTE, "L2Cap-SendFrame: Channel 0x%04x, Len 0x%x, DataSize 0x%x",
		pHeader->CID, pHeader->Length, DataSize);

	if(pHeader->Length != DataSize) {
		LOG(WIIMOTE, "Faulty packet. It is dropped.");
		return;
	}

	switch (pHeader->CID)
	{
	case 0x0001:
		SignalChannel(pData, DataSize);
		break;

	default:
		{
			CChannelMap::iterator  itr= m_Channel.find(pHeader->CID);
			if (itr != m_Channel.end())
			{
				SChannel& rChannel = itr->second;
				switch(rChannel.PSM)
				{
				case 0x01: //SDP
					{
						HandleSDP(pHeader->CID, pData, DataSize);
					}
					break;

				default:
					PanicAlert("channel %i has unknow PSM %x", pHeader->CID);
					break;
				}
			}
			else
			{
				PanicAlert("SendACLFrame to unknown channel %i", pHeader->CID);
			}

		}
		break;
	}
}

void CWII_IPC_HLE_WiiMote::SendCommandToACL(u8 _Ident, u8 _Code, u8 _CommandLength, u8* _pCommandData)
{
	u8 DataFrame[1024];
	u32 Offset = 0;

	SL2CAP_Header* pHeader = (SL2CAP_Header*)&DataFrame[Offset]; Offset += sizeof(SL2CAP_Header);
	pHeader->CID = 0x0001;
	pHeader->Length = sizeof(SL2CAP_Command) + _CommandLength;

	SL2CAP_Command* pCommand = (SL2CAP_Command*)&DataFrame[Offset]; Offset += sizeof(SL2CAP_Command);
	pCommand->len = _CommandLength; 
	pCommand->ident = _Ident;
	pCommand->code = _Code; 

	memcpy(&DataFrame[Offset], _pCommandData, _CommandLength);

	LOG(WIIMOTE, "  SendCommandToACL (answer)");
	LOG(WIIMOTE, "    Ident: 0x%02x", _Ident);
	LOG(WIIMOTE, "    Code: 0x%02x", _Code);

	// send ....
	m_pHost->SendACLFrame(GetConnectionHandle(), DataFrame, pHeader->Length + sizeof(SL2CAP_Header));


	// dump raw data
	{
		LOG(WIIMOTE, "m_pHost->SendACLFrame: 0x%x", GetConnectionHandle());
		std::string Temp;
		for (u32 j=0; j<pHeader->Length + sizeof(SL2CAP_Header); j++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", DataFrame[j]);
			Temp.append(Buffer);
		}
		LOG(WIIMOTE, "   Data: %s", Temp.c_str());
	}
}

void CWII_IPC_HLE_WiiMote::Connect() 
{
	SendConnectionRequest(HID_OUTPUT_SCID, HIDP_OUTPUT_CHANNEL);
	SendConnectionRequest(HID_INPUT_SCID, HIDP_INPUT_CHANNEL);
}

void CWII_IPC_HLE_WiiMote::SendConnectionRequest(u16 scid, u16 psm) 
{
	// create the channel
	SChannel& rChannel = m_Channel[scid];
	rChannel.PSM = psm;
	rChannel.SCID = scid;

	l2cap_conn_req cr;
	cr.psm = psm;
	cr.scid = scid;

	LOG(WIIMOTE, "  SendConnectionRequest()");
	LOG(WIIMOTE, "    Psm: 0x%04x", cr.psm);
	LOG(WIIMOTE, "    Scid: 0x%04x", cr.scid);

	SendCommandToACL(L2CAP_CONN_REQ, L2CAP_CONN_REQ, sizeof(l2cap_conn_req), (u8*)&cr);
}

void CWII_IPC_HLE_WiiMote::SendConfigurationRequest(u16 scid) {
	_dbg_assert_(WIIMOTE, DoesChannelExist(scid));
	SChannel& rChannel = m_Channel[scid];

	u8 Buffer[1024];
	int Offset = 0;

	l2cap_conf_req* cr = (l2cap_conf_req*)Buffer;
	cr->dcid = rChannel.DCID;
	cr->flags = 0;	//what goes here? check libogc.

	Offset += 2;

	/*
	controller doesnt know this...
	Buffer[Offset++] = 1;
	Buffer[Offset++] = 2;
	Buffer[Offset++] = 0;
	Buffer[Offset++] = 1;*/

	Buffer[Offset++] = 2;
	Buffer[Offset++] = 2;
	Buffer[Offset++] = 0xff;
	Buffer[Offset++] = 0xff;


	LOG(WIIMOTE, "  SendConfigurationRequest()");
	LOG(WIIMOTE, "    Dcid: 0x%04x", cr->dcid);
	LOG(WIIMOTE, "    Flags: 0x%04x", cr->flags);

	SendCommandToACL(L2CAP_CONF_REQ, L2CAP_CONF_REQ, Offset, Buffer);
}


void CWII_IPC_HLE_WiiMote::SignalChannel(u8* _pData, u32 _Size)
{    
	while (_Size >= sizeof(SL2CAP_Command)) 
	{
		SL2CAP_Command* pCommand = (SL2CAP_Command*)_pData;
		_pData += sizeof(SL2CAP_Command);
		_Size = _Size - sizeof(SL2CAP_Command) - pCommand->len;

		switch(pCommand->code)
		{
		case L2CAP_CONN_REQ:            
			CommandConnectionReq(pCommand->ident,  _pData, pCommand->len);
			break;

		case L2CAP_CONF_REQ:            
			CommandCofigurationReq(pCommand->ident,  _pData, pCommand->len);
			break;

		case L2CAP_CONN_RSP:
			CommandConnectionResponse(pCommand->ident,  _pData, pCommand->len);
			break;

		case L2CAP_DISCONN_REQ:
			CommandDisconnectionReq(pCommand->ident,  _pData, pCommand->len);
			PanicAlert("SignalChannel - L2CAP_DISCONN_REQ (something went wrong)",pCommand->code);
			break;

		case L2CAP_CONF_RSP:
			CommandCofigurationResponse(pCommand->ident, _pData, pCommand->len);
			break;

		case L2CAP_COMMAND_REJ:
			PanicAlert("SignalChannel - L2CAP_COMMAND_REJ (something went wrong)",pCommand->code);
			break;

		default:
			LOG(WIIMOTE, "  Unknown Command-Code (0x%02x)", pCommand->code);
			PanicAlert("SignalChannel %x",pCommand->code);
			return;
		}

		_pData += pCommand->len;
	}
}


void CWII_IPC_HLE_WiiMote::HidOutput(u8* _pData, u32 _Size)
{    
	PluginWiimote::Wiimote_Output(_pData, _Size);

	//return handshake
	u8 handshake = 0;
	SendL2capData(HID_OUTPUT_SCID, &handshake, 1);
}

void CWII_IPC_HLE_WiiMote::SendL2capData(u16 scid, const void* _pData, u32 _Size)
{
	//allocate
	u8 DataFrame[1024];
	u32 Offset = 0;
	SL2CAP_Header* pHeader = (SL2CAP_Header*)DataFrame;
	Offset += sizeof(SL2CAP_Header);

	_dbg_assert_(WIIMOTE, DoesChannelExist(scid));
	SChannel& rChannel = m_Channel[scid];

	//assemble
	pHeader->CID = rChannel.DCID;
	pHeader->Length = _Size;

	memcpy(DataFrame + Offset, _pData, _Size);
	Offset += _Size;

	//send
	m_pHost->SendACLFrame(GetConnectionHandle(), DataFrame, Offset);
}

void CWII_IPC_HLE_WiiMote::CommandConnectionReq(u8 _Ident, u8* _pData, u32 _Size)
{
	SL2CAP_CommandConnectionReq* pCommandConnectionReq = (SL2CAP_CommandConnectionReq*)_pData;

	// create the channel
	SChannel& rChannel = m_Channel[pCommandConnectionReq->scid];
	rChannel.PSM = pCommandConnectionReq->psm;
	rChannel.SCID = pCommandConnectionReq->scid;
	rChannel.DCID = pCommandConnectionReq->scid;

	LOG(WIIMOTE, "  CommandConnectionReq");
	LOG(WIIMOTE, "    Ident: 0x%02x", _Ident);
	LOG(WIIMOTE, "    PSM: 0x%04x", rChannel.PSM);
	LOG(WIIMOTE, "    SCID: 0x%04x", rChannel.SCID);
	LOG(WIIMOTE, "    DCID: 0x%04x", rChannel.DCID);

	// response
	SL2CAP_ConnectionResponse Rsp;
	Rsp.scid   = rChannel.SCID;
	Rsp.dcid   = rChannel.DCID;
	Rsp.result = 0x00;
	Rsp.status = 0x00;

	SendCommandToACL(_Ident, L2CAP_CONN_RSP, sizeof(SL2CAP_ConnectionResponse), (u8*)&Rsp);
}

class CConfigurationResponse
{
public:
	CConfigurationResponse(u8 identifier, u16 scid, u16 flags, u16 result)
	{
		buffer[0] = 0x05;
		buffer[1] = identifier;
		*(u16*)&buffer[4] = scid;
		*(u16*)&buffer[6] = flags;
		*(u16*)&buffer[8] = result;

		length = 10;

		UpdateLen();
	}

	void AddConfig(u8 type, u8 len, void* data)
	{
		buffer[length++] = type;
		buffer[length++] = len;
		memcpy(&buffer[length], data, len);
		length += len;

		UpdateLen();
	}

	u16 getLen() { return length; }
	u8* getBuffer() { return buffer; }

private:

	void UpdateLen()
	{
		*(u16*)&buffer[2] = length;
	}

	u8 buffer[1024];
	u16 length;
};

void CWII_IPC_HLE_WiiMote::CommandCofigurationReq(u8 _Ident, u8* _pData, u32 _Size)
{
	u32 Offset = 0;
	SL2CAP_CommandConfigurationReq* pCommandConfigReq = (SL2CAP_CommandConfigurationReq*)_pData;

	_dbg_assert_(WIIMOTE, pCommandConfigReq->flags == 0x00); // 1 means that the options are send in multi-packets

	_dbg_assert_(WIIMOTE, DoesChannelExist(pCommandConfigReq->dcid));
	SChannel& rChanel = m_Channel[pCommandConfigReq->dcid];

	LOG(WIIMOTE, "  CommandCofigurationReq");
	LOG(WIIMOTE, "    Ident: 0x%02x", _Ident);
	LOG(WIIMOTE, "    DCID: 0x%04x", pCommandConfigReq->dcid);
	LOG(WIIMOTE, "    Flags: 0x%04x", pCommandConfigReq->flags);

	Offset += sizeof(SL2CAP_CommandConfigurationReq);


	u8 TempBuffer[1024];
	u32 RespLen = 0;

	SL2CAP_CommandConfigurationResponse* Rsp = (SL2CAP_CommandConfigurationResponse*)TempBuffer;
	Rsp->scid   = rChanel.DCID;
	Rsp->flags  = 0x00;
	Rsp->result = 0x00;

	RespLen += sizeof(SL2CAP_CommandConfigurationResponse);



	// prolly this code should be inside the channel...
	while (Offset < _Size)
	{
		SL2CAP_Options* pOptions = (SL2CAP_Options*)&_pData[Offset];
		Offset += sizeof(SL2CAP_Options);

		switch(pOptions->type)
		{
		case 0x01:
			{
				_dbg_assert_(WIIMOTE, pOptions->length == 2);
				SL2CAP_OptionsMTU* pMTU = (SL2CAP_OptionsMTU*)&_pData[Offset];
				rChanel.MTU = pMTU->MTU;
				LOG(WIIMOTE, "    Config MTU: 0x%04x", pMTU->MTU);
			}
			break;

		case 0x02:
			{
				_dbg_assert_(WIIMOTE, pOptions->length == 2);
				SL2CAP_OptionsFlushTimeOut* pFlushTimeOut = (SL2CAP_OptionsFlushTimeOut*)&_pData[Offset];
				rChanel.FlushTimeOut = pFlushTimeOut->TimeOut;
				LOG(WIIMOTE, "    Config FlushTimeOut: 0x%04x", pFlushTimeOut->TimeOut);
			}
			break;

		default:
			_dbg_assert_msg_(WIIMOTE, 0, "Unknown Option: 0x%02x", pOptions->type);
			break;
		}

		Offset += pOptions->length;

		u32 OptionSize = sizeof(SL2CAP_Options) + pOptions->length;
		memcpy(&TempBuffer[RespLen], pOptions, OptionSize);
		RespLen += OptionSize;
	}


	SendCommandToACL(_Ident, L2CAP_CONF_RSP, RespLen, TempBuffer);

	// ugly
	SendConfigurationRequest(Rsp->scid);
}

void CWII_IPC_HLE_WiiMote::CommandConnectionResponse(u8 _Ident, u8* _pData, u32 _Size)
{
	l2cap_conn_rsp* rsp = (l2cap_conn_rsp*)_pData;

	_dbg_assert_(WIIMOTE, _Size == sizeof(l2cap_conn_rsp));

	LOG(WIIMOTE, "  CommandConnectionResponse");
	LOG(WIIMOTE, "    DCID: 0x%04x", rsp->dcid);
	LOG(WIIMOTE, "    SCID: 0x%04x", rsp->scid);
 	LOG(WIIMOTE, "    Result: 0x%04x", rsp->result);
	LOG(WIIMOTE, "    Status: 0x%04x", rsp->status);

	_dbg_assert_(WIIMOTE, rsp->result == 0);
	_dbg_assert_(WIIMOTE, rsp->status == 0);

	_dbg_assert_(WIIMOTE, DoesChannelExist(rsp->scid));
	SChannel& rChannel = m_Channel[rsp->scid];
	rChannel.DCID = rsp->dcid;

	SendConfigurationRequest(rsp->scid);
}

void CWII_IPC_HLE_WiiMote::CommandDisconnectionReq(u8 _Ident, u8* _pData, u32 _Size)
{
	SL2CAP_CommandDisconnectionReq* pCommandDisconnectionReq = (SL2CAP_CommandDisconnectionReq*)_pData;

	// create the channel
	_dbg_assert_(WIIMOTE, m_Channel.find(pCommandDisconnectionReq->scid) != m_Channel.end());

	LOG(WIIMOTE, "  CommandDisconnectionReq");
	LOG(WIIMOTE, "    Ident: 0x%02x", _Ident);
	LOG(WIIMOTE, "    SCID: 0x%04x", pCommandDisconnectionReq->dcid);
	LOG(WIIMOTE, "    DCID: 0x%04x", pCommandDisconnectionReq->scid);

	// response
	SL2CAP_CommandDisconnectionResponse Rsp;
	Rsp.scid   = pCommandDisconnectionReq->scid;
	Rsp.dcid   = pCommandDisconnectionReq->dcid;

	SendCommandToACL(_Ident, L2CAP_DISCONN_RSP, sizeof(SL2CAP_CommandDisconnectionResponse), (u8*)&Rsp);
}



void CWII_IPC_HLE_WiiMote::CommandCofigurationResponse(u8 _Ident, u8* _pData, u32 _Size) {
#ifdef LOGGING
	l2cap_conf_rsp* rsp = (l2cap_conf_rsp*)_pData;
#endif

	_dbg_assert_(WIIMOTE, _Size == sizeof(l2cap_conf_rsp));

	LOG(WIIMOTE, "  CommandCofigurationResponse");
	LOG(WIIMOTE, "    SCID: 0x%04x", rsp->scid);
	LOG(WIIMOTE, "    Flags: 0x%04x", rsp->flags);
 	LOG(WIIMOTE, "    Result: 0x%04x", rsp->result);

	_dbg_assert_(WIIMOTE, rsp->result == 0);
}





/////////////////////////////////////////////////////////////////////////////////////////////////

#define SDP_UINT8  		0x08
#define SDP_UINT16		0x09
#define SDP_UINT32		0x0A
#define SDP_SEQ8		0x35
#define SDP_SEQ16		0x36




void CWII_IPC_HLE_WiiMote::SDPSendServiceSearchResponse(u16 cid, u16 TransactionID, u8* pServiceSearchPattern, u16 MaximumServiceRecordCount)
{
	// verify block... we hanlde search pattern for HID service only
	{
		CBigEndianBuffer buffer(pServiceSearchPattern);		
		_dbg_assert_(WIIMOTE, buffer.Read8(0) == SDP_SEQ8);       // data sequence
		_dbg_assert_(WIIMOTE, buffer.Read8(1) == 0x03);		  // sequence size

		// HIDClassID
		_dbg_assert_(WIIMOTE, buffer.Read8(2) == 0x19);
		_dbg_assert_(WIIMOTE, buffer.Read16(3) == 0x1124);
	}

	u8 DataFrame[1000];
	CBigEndianBuffer buffer(DataFrame);

	int Offset = 0;
	SL2CAP_Header* pHeader = (SL2CAP_Header*)&DataFrame[Offset]; Offset += sizeof(SL2CAP_Header);
	pHeader->CID = cid;

	buffer.Write8 (Offset, 0x03);				Offset++;
	buffer.Write16(Offset, TransactionID);		Offset += 2;		// transaction ID
	buffer.Write16(Offset, 0x0009);				Offset += 2;		// param length
	buffer.Write16(Offset, 0x0001);				Offset += 2;		// TotalServiceRecordCount
	buffer.Write16(Offset, 0x0001);				Offset += 2;		// CurrentServiceRecordCount	
	buffer.Write32(Offset, 0x10000);			Offset += 4;		// ServiceRecordHandleList[4]
	buffer.Write8(Offset, 0x00);				Offset++;			// no continuation state;


	pHeader->Length = Offset - sizeof(SL2CAP_Header);
	m_pHost->SendACLFrame(GetConnectionHandle(), DataFrame, pHeader->Length + sizeof(SL2CAP_Header));
}

u32 ParseCont(u8* pCont)
{
	u32 attribOffset = 0;
	CBigEndianBuffer attribList(pCont);
	u8 typeID      = attribList.Read8(attribOffset);		attribOffset++; 

	if (typeID == 0x02)
	{
		return attribList.Read16(attribOffset);
	}
	else if (typeID == 0x00)
	{
		return 0x00;
	}

	PanicAlert("wrong cont: %i", typeID);
	return 0;
}


int ParseAttribList(u8* pAttribIDList, u16& _startID, u16& _endID)
{
	u32 attribOffset = 0;
	CBigEndianBuffer attribList(pAttribIDList);

	u8 sequence		= attribList.Read8(attribOffset);		attribOffset++;  _dbg_assert_(WIIMOTE, sequence == SDP_SEQ8);
	u8 seqSize		= attribList.Read8(attribOffset);		attribOffset++;
	u8 typeID      = attribList.Read8(attribOffset);		attribOffset++; 

	if (typeID == SDP_UINT32)
	{
		_startID = attribList.Read16(attribOffset);			attribOffset += 2;
		_endID = attribList.Read16(attribOffset);			attribOffset += 2;
	}
	else
	{
		_startID = attribList.Read16(attribOffset);		attribOffset += 2;
		_endID = _startID;
		PanicAlert("Read just a single attrib - not tested");
	}

	return attribOffset;
}


u8 g_Buffer[2048];
u32 g_Size = 0;


u32 GenerateAttribBuffer(u16 _startID, u16 _endID)
{
	CBigEndianBuffer buffer(g_Buffer);
	int Offset = 0;

	buffer.Write8(Offset, SDP_SEQ16); Offset++;

	Offset += 2; // save some memory for seq size

	// walk through the table 
	const CAttribTable& rAttribTable = GetAttribTable();
	CAttribTable::const_iterator itr = rAttribTable.begin();

	u32 sequenceSize = 0;
	while(itr != rAttribTable.end())
	{
		const SAttrib& rAttrib = *itr;

		if ((rAttrib.ID >= _startID) && (rAttrib.ID <= _endID))
		{
			_dbg_assert_(WIIMOTE, rAttrib.size <= 230);

			// ATTRIB TYPE ID
			buffer.Write8(Offset, SDP_UINT16);		Offset ++;
			buffer.Write16(Offset, rAttrib.ID);		Offset += 2;
			sequenceSize += 3;

			// RAW ATTRIB DATA
			memcpy(buffer.GetPointer(Offset), rAttrib.pData, rAttrib.size);
			Offset += rAttrib.size;

			sequenceSize += rAttrib.size;
		}
		itr++;
	}

	buffer.Write16(1, sequenceSize);	

	g_Size = Offset;

	

	return g_Size;
}


void CWII_IPC_HLE_WiiMote::SDPSendServiceAttributeResponse(u16 cid, u16 TransactionID, u32 ServiceHandle, 
														   u16 startAttrID, u16 endAttrID, 
														   u16 MaximumAttributeByteCount, u8* pContinuationState)
{
	if (ServiceHandle != 0x10000)
	{
		PanicAlert("unknown service handle %x" , ServiceHandle);
	}


//	_dbg_assert_(WIIMOTE, ServiceHandle == 0x10000);

	u32 contState = ParseCont(pContinuationState);

	u32 packetSize = 0;
	const u8* pPacket = GetAttribPacket(ServiceHandle, contState, packetSize);

	// generate package
	u8 DataFrame[1000];
	CBigEndianBuffer buffer(DataFrame);

	int Offset = 0;
	SL2CAP_Header* pHeader = (SL2CAP_Header*)&DataFrame[Offset]; Offset += sizeof(SL2CAP_Header);
	pHeader->CID = cid;
		
	buffer.Write8 (Offset, 0x05);										Offset++;
	buffer.Write16(Offset, TransactionID);								Offset += 2;			// transaction ID

	memcpy(buffer.GetPointer(Offset), pPacket, packetSize);				Offset += packetSize;



	pHeader->Length = Offset - sizeof(SL2CAP_Header);
	m_pHost->SendACLFrame(GetConnectionHandle(), DataFrame, pHeader->Length + sizeof(SL2CAP_Header));


	// dump raw data
	{
		LOG(WIIMOTE, "test response: 0x%x", GetConnectionHandle());		
		for (u32 j=0; j<pHeader->Length + sizeof(SL2CAP_Header);)
		{
			std::string Temp;
			for (int i=0; i<16; i++)
			{
				char Buffer[128];
				sprintf(Buffer, "%02x ", DataFrame[j++]);
				Temp.append(Buffer);

				if (j >= pHeader->Length + sizeof(SL2CAP_Header))
					break;
			}

			LOG(WIIMOTE, "   Data: %s", Temp.c_str());
		}
		
	}

	
}

void CWII_IPC_HLE_WiiMote::HandleSDP(u16 cid, u8* _pData, u32 _Size)
{
	// dump raw data
	{
		LOG(WIIMOTE, "HandleSDP: 0x%x", GetConnectionHandle());
		std::string Temp;
		for (u32 j=0; j<_Size; j++)
		{
			char Buffer[128];
			sprintf(Buffer, "%02x ", _pData[j]);
			Temp.append(Buffer);
		}
		LOG(WIIMOTE, "   Data: %s", Temp.c_str());
	}



	CBigEndianBuffer buffer(_pData);

	switch(buffer.Read8(0))
	{
		// SDP_ServiceSearchRequest
	case 0x02:
		{
			LOG(WIIMOTE, "!!! SDP_ServiceSearchRequest !!!");

			_dbg_assert_(WIIMOTE, _Size == 13);

			u16 TransactionID = buffer.Read16(1);
			u16 ParameterLength = buffer.Read16(3);
			u8* pServiceSearchPattern = buffer.GetPointer(5);
			u16 MaximumServiceRecordCount = buffer.Read16(10);
			u8 ContinuationState = buffer.Read8(12);

			SDPSendServiceSearchResponse(cid, TransactionID, pServiceSearchPattern, MaximumServiceRecordCount);
		}
		break;

		// SDP_ServiceAttributeRequest
	case 0x04:
		{
			LOG(WIIMOTE, "!!! SDP_ServiceAttributeRequest !!!");

			u16 startAttrID, endAttrID;
			u32 offset = 1;

			u16 TransactionID = buffer.Read16(offset);					offset += 2;
			u16 ParameterLength = buffer.Read16(offset);				offset += 2;
			u32 ServiceHandle = buffer.Read32(offset);					offset += 4;
			u16 MaximumAttributeByteCount = buffer.Read16(offset);		offset += 2;
			offset += ParseAttribList(buffer.GetPointer(offset), startAttrID, endAttrID);
			u8* pContinuationState =  buffer.GetPointer(offset);

			SDPSendServiceAttributeResponse(cid, TransactionID, ServiceHandle, startAttrID, endAttrID, MaximumAttributeByteCount, pContinuationState);
		}
		break;

	default:
		PanicAlert("Unknown SDP command %x", _pData[0]);
		break;
	}
}	