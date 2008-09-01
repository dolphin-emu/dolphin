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

#include "WII_IPC_HLE_Device_usb.h"
#include <vector>


#define USB_HLE_LOG DSPHLE

CWII_IPC_HLE_Device_usb_oh1_57e_305::CWII_IPC_HLE_Device_usb_oh1_57e_305(u32 _DeviceID, const std::string& _rDeviceName)
	: IWII_IPC_HLE_Device(_DeviceID, _rDeviceName)
    , m_ACLAnswer(false)
    , m_pACLBuffer(NULL)
    , m_pHCIBuffer(NULL)
    , m_State(STATE_NONE)
    , scan_enable(0)
{
	m_WiiMotes.push_back(CWII_IPC_HLE_WiiMote(this, 0));

	m_ControllerBD.b[0] = 0x11;
	m_ControllerBD.b[1] = 0x02;
	m_ControllerBD.b[2] = 0x19;
	m_ControllerBD.b[3] = 0x79;
	m_ControllerBD.b[4] = 0x00;
	m_ControllerBD.b[5] = 0xFF;
}

CWII_IPC_HLE_Device_usb_oh1_57e_305::~CWII_IPC_HLE_Device_usb_oh1_57e_305()
{}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::Open(u32 _CommandAddress)
{
    Memory::Write_U32(GetDeviceID(), _CommandAddress+4);
    return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::IOCtl(u32 _CommandAddress)
{
	return IOCtlV(_CommandAddress);	//hack
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::IOCtlV(u32 _CommandAddress) 
{	
    // wpadsampled.elf - patch so the USB_LOG will print somehting 
	// even it it wasn't very useful yet...
    // Memory::Write_U8(1, 0x801514A8); // USB_LOG
	Memory::Write_U8(1, 0x801514D8); // WUD_DEBUGPrint
    
    SIOCtlVBuffer CommandBuffer(_CommandAddress);

	// DumpAsync(CommandBuffer.BufferVector, _CommandAddress, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);

	switch(CommandBuffer.Parameter)
	{
	case USB_IOCTL_HCI_COMMAND_MESSAGE:
		{
			SHCICommandMessage CtrlSetup;

            // the USB stuff is little endian
			CtrlSetup.bRequestType = *(u8*)Memory::GetPointer(CommandBuffer.InBuffer[0].m_Address);
			CtrlSetup.bRequest = *(u8*)Memory::GetPointer(CommandBuffer.InBuffer[1].m_Address);
			CtrlSetup.wValue = *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[2].m_Address);
			CtrlSetup.wIndex = *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[3].m_Address);
			CtrlSetup.wLength = *(u16*)Memory::GetPointer(CommandBuffer.InBuffer[4].m_Address);
#ifdef _DEBUG
			u8 Termination =*(u8*)Memory::GetPointer(CommandBuffer.InBuffer[5].m_Address);
#endif
            CtrlSetup.m_PayLoadAddr = CommandBuffer.PayloadBuffer[0].m_Address;
            CtrlSetup.m_PayLoadSize = CommandBuffer.PayloadBuffer[0].m_Size;

            _dbg_assert_msg_(USB_HLE_LOG, Termination == 0, "USB_HLE_LOG: Termination != 0");

			LOG(USB_HLE_LOG, "USB_IOCTL_CTRLMSG (0x%08x) - add to queue and send ack only", _CommandAddress);

/*			LOG(USB_HLE_LOG, "    bRequestType: 0x%x", CtrlSetup.bRequestType);
			LOG(USB_HLE_LOG, "    bRequest: 0x%x", CtrlSetup.bRequest);
			LOG(USB_HLE_LOG, "    wValue: 0x%x", CtrlSetup.wValue);
			LOG(USB_HLE_LOG, "    wIndex: 0x%x", CtrlSetup.wIndex);
			LOG(USB_HLE_LOG, "    wLength: 0x%x", CtrlSetup.wLength); */
            
            m_HCICommandMessageQueue.push(CtrlSetup);

            // control message has been sent...
            Memory::Write_U32(0, _CommandAddress + 0x4);
            return true;
		}
		break;

	case USB_IOCTL_BLKMSG:
		{
			u8 Command = Memory::Read_U8(CommandBuffer.InBuffer[0].m_Address);
			switch (Command)
			{
            case ACL_DATA_ENDPOINT_READ:
                {
                    // write
                    DumpAsync(CommandBuffer.BufferVector, _CommandAddress, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);

                    SIOCtlVBuffer pBulkBuffer(_CommandAddress);
                    UACLHeader* pACLHeader = (UACLHeader*)Memory::GetPointer(pBulkBuffer.PayloadBuffer[0].m_Address);
                   
					_dbg_assert_(USB_HLE_LOG, pACLHeader->BCFlag == 0);
					_dbg_assert_(USB_HLE_LOG, pACLHeader->PBFlag == 2);

                    SendToDevice(pACLHeader->ConnectionHandle, Memory::GetPointer(pBulkBuffer.PayloadBuffer[0].m_Address + 4), pACLHeader->Size);
                }
                break;

			case ACL_DATA_ENDPOINT:
				{
                    if (m_pACLBuffer)
                        delete m_pACLBuffer;
                    m_pACLBuffer = new SIOCtlVBuffer(_CommandAddress);

					LOG(USB_HLE_LOG, "ACL_DATA_ENDPOINT: 0x%08x ", _CommandAddress);
				    return false;
				}
				break;

			default:
				{
					_dbg_assert_msg_(USB_HLE_LOG, 0, "Unknown USB_IOCTL_BLKMSG: %x", Command);		
				}
				break;
			}
		}
		break;


	case USB_IOCTL_INTRMSG:
		{ 
			u8 Command = Memory::Read_U8(CommandBuffer.InBuffer[0].m_Address);
			switch (Command)
			{
			case HCI_EVENT_ENDPOINT:
				{
                    if (m_pHCIBuffer)
                        delete m_pHCIBuffer;
                    m_pHCIBuffer = new SIOCtlVBuffer(_CommandAddress);

					LOG(USB_HLE_LOG, "HCI_EVENT_ENDPOINT: 0x%08x", _CommandAddress);
                    return false;							
   				}
				break;

			default:
				{
					_dbg_assert_msg_(USB_HLE_LOG, 0, "Unknown USB_IOCTL_INTRMSG: %x", Command);		
				}
				break;
			}
		}
		break;

	default:
		{
			_dbg_assert_msg_(USB_HLE_LOG, 0, "Unknown CWII_IPC_HLE_Device_usb_oh1_57e_305: %x", CommandBuffer.Parameter);

            LOG(USB_HLE_LOG, "%s - IOCtlV:", GetDeviceName().c_str());
            LOG(USB_HLE_LOG, "    Parameter: 0x%x", CommandBuffer.Parameter);
            LOG(USB_HLE_LOG, "    NumberIn: 0x%08x", CommandBuffer.NumberInBuffer);
            LOG(USB_HLE_LOG, "    NumberOut: 0x%08x", CommandBuffer.NumberPayloadBuffer);
            LOG(USB_HLE_LOG, "    BufferVector: 0x%08x", CommandBuffer.BufferVector);
            LOG(USB_HLE_LOG, "    BufferSize: 0x%08x", CommandBuffer.BufferSize); 
            DumpAsync(CommandBuffer.BufferVector, _CommandAddress, CommandBuffer.NumberInBuffer, CommandBuffer.NumberPayloadBuffer);
		}
		break;
	}

    // write return value
    Memory::Write_U32(0, _CommandAddress + 0x4);

    return true;
}

extern void SendFrame(CWII_IPC_HLE_Device_usb_oh1_57e_305* _pDevice, u16 _ConnectionHandle, u8* _pData, u32 _Size);

void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendToDevice(u16 _ConnectionHandle, u8* _pData, u32 _Size)
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(_ConnectionHandle);
	if (pWiiMote == NULL)
	{
		PanicAlert("Cant find WiiMote by connection handle: %02x", _ConnectionHandle);
		return;
	}

    pWiiMote->SendACLFrame(_pData, _Size);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendACLFrame(u16 _ConnectionHandle, u8* _pData, u32 _Size)
{
    UACLHeader* pHeader = (UACLHeader*)Memory::GetPointer(m_pACLBuffer->PayloadBuffer[0].m_Address);
    pHeader->ConnectionHandle = _ConnectionHandle;
    pHeader->BCFlag = 0;
    pHeader->PBFlag = 2;
    pHeader->Size = _Size;

    memcpy(Memory::GetPointer(m_pACLBuffer->PayloadBuffer[0].m_Address + sizeof(UACLHeader)), _pData, _Size);

    // return reply buffer size
    Memory::Write_U32(sizeof(UACLHeader) + _Size, m_pACLBuffer->m_Address + 0x4);

    m_ACLAnswer = true;
}

u32 CWII_IPC_HLE_Device_usb_oh1_57e_305::Update() 
{ 
	// check state machine
    if (m_pHCIBuffer)
    {
        bool ReturnHCIBuffer = false;
        switch(m_State)
        {
        case STATE_NONE:
            break;

        case STATE_INQUIRY_RESPONSE:
            SendEventInquiryResponse();
            m_State = STATE_INQUIRY_COMPLETE;
            ReturnHCIBuffer = true;
            break;

        case STATE_INQUIRY_COMPLETE:
            SendEventInquiryComplete();
            m_State = STATE_NONE;
            ReturnHCIBuffer = true;
            break;

		case STATE_REMOTE_NAME_REQ:
			SendEventRemoteNameReq();
			m_State = STATE_NONE;
            ReturnHCIBuffer = true;
            break;

		case STATE_CONNECTION_COMPLETE_EVENT:
			SendEventConnectionComplete();
			m_State = STATE_NONE;
            ReturnHCIBuffer = true;
            break;

        case STATE_READ_CLOCK_OFFSET:
            SendEventReadClockOffsetComplete();
            m_State = STATE_NONE;
            ReturnHCIBuffer = true;
            break;

        case STATE_READ_REMOTE_VER_INFO:
            SendEventReadRemoteVerInfo();
            m_State = STATE_NONE;
            ReturnHCIBuffer = true;
            break;

        case STATE_READ_REMOTE_FEATURES:
            SendEventReadRemoteFeatures();
            m_State = STATE_NONE;
            ReturnHCIBuffer = true;
            break;
           
        default:
            PanicAlert("Unknown State in USBDev");
            break;
        }

        if (ReturnHCIBuffer)
        {
            u32 Addr = m_pHCIBuffer->m_Address;
            delete m_pHCIBuffer;
            m_pHCIBuffer = NULL;
            return Addr;
        }
    }

	// HCI control message/event handling
	if (!m_HCICommandMessageQueue.empty() && m_pHCIBuffer)
	{
		const SHCICommandMessage& rMessage = m_HCICommandMessageQueue.front();

		ExecuteHCICommandMessage(rMessage);
		m_HCICommandMessageQueue.pop(); 

		u32 Addr = m_pHCIBuffer->m_Address;
		delete m_pHCIBuffer;
		m_pHCIBuffer = NULL;
		return Addr;
	}

	// check if something is inside the ACL Buffer
	if (m_ACLAnswer && m_pACLBuffer)
	{
		m_ACLAnswer = false;

		u32 Addr = m_pACLBuffer->m_Address;
		delete m_pACLBuffer;
		m_pACLBuffer = NULL;
		return Addr;
	}

	return 0; 
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventCommandStatus(u16 _Opcode)
{        
    SHCIEventStatus* pHCIEvent = (SHCIEventStatus*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    

    pHCIEvent->EventType = 0x0F;
	pHCIEvent->PayloadLength = 0x4;
    pHCIEvent->Status = 0x0;
	pHCIEvent->PacketIndicator = 0x01;
	pHCIEvent->Opcode = _Opcode;

    // return reply buffer size
    Memory::Write_U32(sizeof(SHCIEventStatus), m_pHCIBuffer->m_Address + 0x4);

    return true;
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventCommandComplete(u16 _OpCode, void* _pData, u32 _DataSize)
{
	SHCIEventCommand* pHCIEvent = (SHCIEventCommand*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    
	pHCIEvent->EventType = 0x0E;          
	pHCIEvent->PayloadLength = _DataSize + 0x03;
	pHCIEvent->PacketIndicator = 0x01;     
	pHCIEvent->Opcode = _OpCode;

	// add the payload
	if ((_pData != NULL) && (_DataSize > 0))
	{
		u8* pPayload = (u8*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address + sizeof(SHCIEventCommand)); 
		memcpy(pPayload, _pData, _DataSize);
	}

	// answer with the number of sent bytes
	Memory::Write_U32(_DataSize + sizeof(SHCIEventCommand), m_pHCIBuffer->m_Address + 0x4);
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventInquiryResponse()
{    
	if (m_WiiMotes.empty())
		return false;

    SHCIEventInquiryResult* pInquiryResult = (SHCIEventInquiryResult*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    

    pInquiryResult->EventType = 0x02;
	pInquiryResult->PayloadLength = 1 + m_WiiMotes.size() * sizeof(hci_inquiry_response); 
    pInquiryResult->num_responses = m_WiiMotes.size();

	for (size_t i=0; i<m_WiiMotes.size(); i++)
	{
		u32 Address = m_pHCIBuffer->PayloadBuffer[0].m_Address + sizeof(SHCIEventInquiryResult) + i*sizeof(hci_inquiry_response);
		hci_inquiry_response* pResponse = (hci_inquiry_response*)Memory::GetPointer(Address);   

		pResponse->bdaddr = m_WiiMotes[i].GetBD();
		pResponse->uclass[0]= m_WiiMotes[i].GetClass()[0];
		pResponse->uclass[1]= m_WiiMotes[i].GetClass()[1];
		pResponse->uclass[2]= m_WiiMotes[i].GetClass()[2];

		pResponse->page_scan_rep_mode = 1;
		pResponse->page_scan_period_mode = 0;
		pResponse->page_scan_mode = 0;
		pResponse->clock_offset = 0x3818;

		LOG(USB_HLE_LOG, "Event: Send Fake Inquriy of one controller");
		LOG(USB_HLE_LOG, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", 
			pResponse->bdaddr.b[0], pResponse->bdaddr.b[1], pResponse->bdaddr.b[2],
			pResponse->bdaddr.b[3], pResponse->bdaddr.b[4], pResponse->bdaddr.b[5]);
	}

    // return reply buffer size
    Memory::Write_U32(sizeof(SHCIEventInquiryResult) + sizeof(hci_inquiry_response), m_pHCIBuffer->m_Address + 0x4);

    return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventInquiryComplete()
{
    SHCIEventInquiryComplete* pInquiryComplete = (SHCIEventInquiryComplete*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    
	pInquiryComplete->EventType = 0x01;
	pInquiryComplete->PayloadLength = 0x01; 
    pInquiryComplete->status = 0x00;

    // control message has been sent...
    Memory::Write_U32(sizeof(SHCIEventInquiryComplete), m_pHCIBuffer->m_Address + 0x4);

    LOG(USB_HLE_LOG, "Event: Inquiry complete");

    return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventRemoteNameReq()
{    
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(m_StateTempBD);
	if (pWiiMote == NULL)
	{
		PanicAlert("Cant find WiiMote by bd: %02x:%02x:%02x:%02x:%02x:%02x",
			m_StateTempBD.b[0], m_StateTempBD.b[1], m_StateTempBD.b[2],
			m_StateTempBD.b[3], m_StateTempBD.b[4], m_StateTempBD.b[5]);
		return false;
	}
	ClearBD(m_StateTempBD);

	SHCIEventRemoteNameReq* pRemoteNameReq = (SHCIEventRemoteNameReq*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    

	pRemoteNameReq->EventType = 0x07;
	pRemoteNameReq->PayloadLength = 255;
	pRemoteNameReq->Status = 0x00;
	pRemoteNameReq->bdaddr = pWiiMote->GetBD();
	strcpy((char*)pRemoteNameReq->RemoteName, pWiiMote->GetName());

	// return reply buffer size
	Memory::Write_U32(sizeof(SHCIEventRemoteNameReq), m_pHCIBuffer->m_Address + 0x4);

	LOG(USB_HLE_LOG, "Event: SendEventRemoteNameReq");
	LOG(USB_HLE_LOG, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", 
		pRemoteNameReq->bdaddr.b[0], pRemoteNameReq->bdaddr.b[1], pRemoteNameReq->bdaddr.b[2],
		pRemoteNameReq->bdaddr.b[3], pRemoteNameReq->bdaddr.b[4], pRemoteNameReq->bdaddr.b[5]);
	LOG(USB_HLE_LOG, "  remotename: %s", pRemoteNameReq->RemoteName);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventRequestConnection()
{
	PanicAlert("unused");
/*
	SHCIEventRequestConnection* pEventRequestConnection = (SHCIEventRequestConnection*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    

	pEventRequestConnection->EventType = 0x04;
	pEventRequestConnection->PayloadLength = 13;

	pEventRequestConnection->bdaddr = XXXXX;  // BD_ADDR of the device that requests the connection
	pEventRequestConnection->uclass[0] = 0x04;  // thanks to shagkur
	pEventRequestConnection->uclass[1] = 0x25;
	pEventRequestConnection->uclass[2] = 0x00;
	pEventRequestConnection->LinkType = 0x01;

	// return reply buffer size
	Memory::Write_U32(sizeof(SHCIEventRequestConnection), m_pHCIBuffer->m_Address + 0x4);

	LOG(USB_HLE_LOG, "Event: SendEventRequestConnection");
	LOG(USB_HLE_LOG, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", 
		pEventRequestConnection->bdaddr.b[0], pEventRequestConnection->bdaddr.b[1], pEventRequestConnection->bdaddr.b[2],
		pEventRequestConnection->bdaddr.b[3], pEventRequestConnection->bdaddr.b[4], pEventRequestConnection->bdaddr.b[5]);
	LOG(USB_HLE_LOG, "  COD[0]: 0x%02x", pEventRequestConnection->uclass[0]);
	LOG(USB_HLE_LOG, "  COD[1]: 0x%02x", pEventRequestConnection->uclass[1]);
	LOG(USB_HLE_LOG, "  COD[2]: 0x%02x", pEventRequestConnection->uclass[2]);
	LOG(USB_HLE_LOG, "  LinkType: %i", pEventRequestConnection->LinkType);
*/
	return true;
};

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventConnectionComplete()
{    
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(m_StateTempBD);
	if (pWiiMote == NULL)
	{
		PanicAlert("Cant find WiiMote by bd: %02x:%02x:%02x:%02x:%02x:%02x",
			m_StateTempBD.b[0], m_StateTempBD.b[1], m_StateTempBD.b[2],
			m_StateTempBD.b[3], m_StateTempBD.b[4], m_StateTempBD.b[5]);
		return false;
	}
	ClearBD(m_StateTempBD);

	SHCIEventConnectionComplete* pConnectionComplete = (SHCIEventConnectionComplete*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    

	pConnectionComplete->EventType = 0x03;
	pConnectionComplete->PayloadLength = 13;
	pConnectionComplete->Status = 0x00;
	pConnectionComplete->Connection_Handle = pWiiMote->GetConnectionHandle();
	pConnectionComplete->bdaddr = pWiiMote->GetBD();
	pConnectionComplete->LinkType = 0x01; // ACL
	pConnectionComplete->EncryptionEnabled = 0x00;

	// return reply buffer size
	Memory::Write_U32(sizeof(SHCIEventConnectionComplete), m_pHCIBuffer->m_Address + 0x4);

	LOG(USB_HLE_LOG, "Event: SendEventConnectionComplete");
	LOG(USB_HLE_LOG, "  Connection_Handle: 0x%04x", pConnectionComplete->Connection_Handle);
	LOG(USB_HLE_LOG, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", 
		pConnectionComplete->bdaddr.b[0], pConnectionComplete->bdaddr.b[1], pConnectionComplete->bdaddr.b[2],
		pConnectionComplete->bdaddr.b[3], pConnectionComplete->bdaddr.b[4], pConnectionComplete->bdaddr.b[5]);
	LOG(USB_HLE_LOG, "  LinkType: %i", pConnectionComplete->LinkType);
	LOG(USB_HLE_LOG, "  EncryptionEnabled: %i", pConnectionComplete->EncryptionEnabled);

	return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventReadClockOffsetComplete()
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(m_StateTempConnectionHandle);
	if (pWiiMote == NULL)
	{
		PanicAlert("Cant find WiiMote by connection handle: %02x", m_StateTempConnectionHandle);
		return false;
	}
	m_StateTempConnectionHandle = 0;

    SHCIEventReadClockOffsetComplete* pReadClockOffsetComplete = (SHCIEventReadClockOffsetComplete*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    
    pReadClockOffsetComplete->EventType = 0x1C;
    pReadClockOffsetComplete->PayloadLength = 0x05; 
    pReadClockOffsetComplete->status = 0x00;
    pReadClockOffsetComplete->ConnectionHandle = pWiiMote->GetConnectionHandle();
    pReadClockOffsetComplete->ClockOffset = 0x3818;

    // return reply buffer size
    Memory::Write_U32(sizeof(SHCIEventReadClockOffsetComplete), m_pHCIBuffer->m_Address + 0x4);

    LOG(USB_HLE_LOG, "Event: SendEventConnectionComplete");
    LOG(USB_HLE_LOG, "  Connection_Handle: 0x%04x", pReadClockOffsetComplete->ConnectionHandle);
    LOG(USB_HLE_LOG, "  ClockOffset: 0x%04x", pReadClockOffsetComplete->ClockOffset);

    return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventReadRemoteVerInfo()
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(m_StateTempConnectionHandle);
	if (pWiiMote == NULL)
	{
		PanicAlert("Cant find WiiMote by connection handle: %02x", m_StateTempConnectionHandle);
		return false;
	}
	m_StateTempConnectionHandle = 0;

    SHCIEventReadRemoteVerInfo* pReadRemoteVerInfo = (SHCIEventReadRemoteVerInfo*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    
    pReadRemoteVerInfo->EventType = 0x0C;
    pReadRemoteVerInfo->PayloadLength = 0x05; 
    pReadRemoteVerInfo->status = 0x00;
    pReadRemoteVerInfo->ConnectionHandle = pWiiMote->GetConnectionHandle();
    pReadRemoteVerInfo->lmp_version = 0x2;
    pReadRemoteVerInfo->manufacturer = 0x000F;
    pReadRemoteVerInfo->lmp_subversion = 0x229;

    // control message has been sent...
    Memory::Write_U32(sizeof(SHCIEventReadRemoteVerInfo), m_pHCIBuffer->m_Address + 0x4);

    LOG(USB_HLE_LOG, "Event: SendEventReadReadRemoteVerInfo");
    LOG(USB_HLE_LOG, "  Connection_Handle: 0x%04x", pReadRemoteVerInfo->ConnectionHandle);
    LOG(USB_HLE_LOG, "  lmp_version: 0x%02x", pReadRemoteVerInfo->lmp_version);
    LOG(USB_HLE_LOG, "  manufacturer: 0x%04x", pReadRemoteVerInfo->manufacturer);
    LOG(USB_HLE_LOG, "  lmp_subversion: 0x%04x", pReadRemoteVerInfo->lmp_subversion);

    return true;
}

bool CWII_IPC_HLE_Device_usb_oh1_57e_305::SendEventReadRemoteFeatures()
{
	CWII_IPC_HLE_WiiMote* pWiiMote = AccessWiiMote(m_StateTempConnectionHandle);
	if (pWiiMote == NULL)
	{
		PanicAlert("Cant find WiiMote by connection handle: %02x", m_StateTempConnectionHandle);
		return false;
	}
	m_StateTempConnectionHandle = 0;

    SHCIEventReadRemoteFeatures* pReadRemoteFeatures = (SHCIEventReadRemoteFeatures*)Memory::GetPointer(m_pHCIBuffer->PayloadBuffer[0].m_Address);    
    pReadRemoteFeatures->EventType = 0x0C;
    pReadRemoteFeatures->PayloadLength = 11; 
    pReadRemoteFeatures->status = 0x00;
    pReadRemoteFeatures->ConnectionHandle = pWiiMote->GetConnectionHandle();
    pReadRemoteFeatures->features[0] = pWiiMote->GetFeatures()[0];
    pReadRemoteFeatures->features[1] = pWiiMote->GetFeatures()[1];
    pReadRemoteFeatures->features[2] = pWiiMote->GetFeatures()[2];
    pReadRemoteFeatures->features[3] = pWiiMote->GetFeatures()[3];
    pReadRemoteFeatures->features[4] = pWiiMote->GetFeatures()[4];
    pReadRemoteFeatures->features[5] = pWiiMote->GetFeatures()[5];
    pReadRemoteFeatures->features[6] = pWiiMote->GetFeatures()[6];
    pReadRemoteFeatures->features[7] = pWiiMote->GetFeatures()[7];

    // control message has been sent...
    Memory::Write_U32(sizeof(SHCIEventReadRemoteFeatures), m_pHCIBuffer->m_Address + 0x4);

    LOG(USB_HLE_LOG, "Event: SendEventReadReadRemoteVerInfo");
    LOG(USB_HLE_LOG, "  Connection_Handle: 0x%04x", pReadRemoteFeatures->ConnectionHandle);
    LOG(USB_HLE_LOG, "  features: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",   
        pReadRemoteFeatures->features[0], pReadRemoteFeatures->features[1], pReadRemoteFeatures->features[2],
        pReadRemoteFeatures->features[3], pReadRemoteFeatures->features[4], pReadRemoteFeatures->features[5], 
        pReadRemoteFeatures->features[6], pReadRemoteFeatures->features[7]);

    return true;
}



void CWII_IPC_HLE_Device_usb_oh1_57e_305::ExecuteHCICommandMessage(const SHCICommandMessage& _rHCICommandMessage)
{
    u8* pInput = Memory::GetPointer(_rHCICommandMessage.m_PayLoadAddr + 3);
    SCommandMessage* pMsg = (SCommandMessage*)Memory::GetPointer(_rHCICommandMessage.m_PayLoadAddr);

    switch(pMsg->Opcode)
    {
    // 
    // --- read commandos ---
    //
    case HCI_CMD_RESET:
        CommandReset(pInput);
        break;

    case HCI_CMD_READ_BUFFER_SIZE:
        CommandReadBufferSize(pInput);
        break;

    case HCI_CMD_READ_LOCAL_VER:
        CommandReadLocalVer(pInput);
        break;

    case HCI_CMD_READ_BDADDR:
        CommandReadBDAdrr(pInput);
        break;

    case HCI_CMD_READ_LOCAL_FEATURES:
        CommandReadLocalFeatures(pInput);
        break;

    case HCI_CMD_READ_STORED_LINK_KEY:
        CommandReadStoredLinkKey(pInput);
        break;

    case HCI_CMD_WRITE_UNIT_CLASS:
        CommandWriteUnitClass(pInput);
        break;

    case HCI_CMD_WRITE_LOCAL_NAME:
        CommandWriteLocalName(pInput);
        break;

    case HCI_CMD_WRITE_PIN_TYPE:
        CommandWritePinType(pInput);
        break;

    case HCI_CMD_HOST_BUFFER_SIZE:
        CommandHostBufferSize(pInput);
        break;

    case HCI_CMD_WRITE_PAGE_TIMEOUT:
        CommandWritePageTimeOut(pInput);
        break;

    case HCI_CMD_WRITE_SCAN_ENABLE:
        CommandWriteScanEnable(pInput);
        break;

    case HCI_CMD_WRITE_INQUIRY_MODE:
        CommandWriteInquiryMode(pInput);
        break;

    case HCI_CMD_WRITE_PAGE_SCAN_TYPE:
        CommandWritePageScanType(pInput);
        break;

    case HCI_CMD_SET_EVENT_FILTER:
        CommandSetEventFilter(pInput);        
        break;

    case HCI_CMD_INQUIRY:
        CommandInquiry(pInput);        
        break;

    case HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:
        CommandWriteInquiryScanType(pInput);        
        break;

    // vendor specific...
    case 0xFC4C:
        CommandVendorSpecific_FC4C(pInput, _rHCICommandMessage.m_PayLoadSize - 3); 
        break;

    case 0xFC4F:
        CommandVendorSpecific_FC4F(pInput, _rHCICommandMessage.m_PayLoadSize - 3); 
        break;

    case HCI_CMD_INQUIRY_CANCEL:
        CommandInquiryCancel(pInput);        
        break;

	case HCI_CMD_REMOTE_NAME_REQ:
		CommandRemoteNameReq(pInput);
		break;

	case HCI_CMD_CREATE_CON:
		CommandCreateCon(pInput);
		break;

	case HCI_CMD_ACCEPT_CON:
		CommandAcceptCon(pInput);
		break;

    case HCI_CMD_READ_CLOCK_OFFSET:
        CommandReadClockOffset(pInput);
        break;
	
    case HCI_CMD_READ_REMOTE_VER_INFO:
        CommandReadRemoteVerInfo(pInput);
        break;

    case HCI_CMD_READ_REMOTE_FEATURES:
        CommandReadRemoteFeatures(pInput);
        break;

    // 
    // --- default ---
    //
    default:
		{
#ifdef _DEBUG
			u16 ocf = HCI_OCF(pMsg->Opcode);
			u16 ogf = HCI_OGF(pMsg->Opcode);
#endif

			_dbg_assert_msg_(USB_HLE_LOG, 0, "Unknown USB_IOCTL_CTRLMSG: 0x%04X (ocf: 0x%x  ogf 0x%x)", pMsg->Opcode, ocf, ogf);

			// send fake all is okay msg...
			SendEventCommandComplete(pMsg->Opcode, NULL, 0);
		}
        break;
    }
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReset(u8* _Input)
{
	// reply
    hci_status_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_RESET, &Reply, sizeof(hci_status_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_RESET");
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadBufferSize(u8* _Input)
{
	// reply
    hci_read_buffer_size_rp Reply;
    Reply.status = 0x00;
    Reply.max_acl_size = 339;
    Reply.num_acl_pkts = 10;
    Reply.max_sco_size = 64;
    Reply.num_sco_pkts = 0;

	SendEventCommandComplete(HCI_CMD_READ_BUFFER_SIZE, &Reply, sizeof(hci_read_buffer_size_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_READ_BUFFER_SIZE:");
    LOG(USB_HLE_LOG, "return:");
    LOG(USB_HLE_LOG, "  max_acl_size: %i", Reply.max_acl_size);
    LOG(USB_HLE_LOG, "  num_acl_pkts: %i", Reply.num_acl_pkts);
    LOG(USB_HLE_LOG, "  max_sco_size: %i", Reply.max_sco_size);
    LOG(USB_HLE_LOG, "  num_sco_pkts: %i", Reply.num_sco_pkts);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadLocalVer(u8* _Input)
{
	// reply
    hci_read_local_ver_rp Reply;
    Reply.status = 0x00;
    Reply.hci_version = 0x03;        // HCI version: 1.1
    Reply.hci_revision = 0x40a7;     // current revision (?)
    Reply.lmp_version = 0x03;        // LMP version: 1.1    
    Reply.manufacturer = 0x000F;     // manufacturer: reserved for tests
    Reply.lmp_subversion = 0x430e;   // LMP subversion

	SendEventCommandComplete(HCI_CMD_READ_LOCAL_VER, &Reply, sizeof(hci_read_local_ver_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_READ_LOCAL_VER:");
    LOG(USB_HLE_LOG, "return:");
    LOG(USB_HLE_LOG, "  status:         %i", Reply.status);
    LOG(USB_HLE_LOG, "  hci_revision:   %i", Reply.hci_revision);
    LOG(USB_HLE_LOG, "  lmp_version:    %i", Reply.lmp_version);
    LOG(USB_HLE_LOG, "  manufacturer:   %i", Reply.manufacturer);
    LOG(USB_HLE_LOG, "  lmp_subversion: %i", Reply.lmp_subversion);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadBDAdrr(u8* _Input)
{
	// reply
    hci_read_bdaddr_rp Reply;
    Reply.status = 0x00;
    Reply.bdaddr = m_ControllerBD;
 
	SendEventCommandComplete(HCI_CMD_READ_BDADDR, &Reply, sizeof(hci_read_bdaddr_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_READ_BDADDR:");
    LOG(USB_HLE_LOG, "return:");
    LOG(USB_HLE_LOG, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", 
                            Reply.bdaddr.b[0], Reply.bdaddr.b[1], Reply.bdaddr.b[2],
                            Reply.bdaddr.b[3], Reply.bdaddr.b[4], Reply.bdaddr.b[5]);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadLocalFeatures(u8* _Input)
{
	// reply
    hci_read_local_features_rp Reply;    
    Reply.status = 0x00;
    Reply.features[0] = 0xFF;
    Reply.features[1] = 0xFF;
    Reply.features[2] = 0x8D;
    Reply.features[3] = 0xFE;
    Reply.features[4] = 0x9B;
    Reply.features[5] = 0xF9;
    Reply.features[6] = 0x00;
    Reply.features[7] = 0x80;

	SendEventCommandComplete(HCI_CMD_READ_LOCAL_FEATURES, &Reply, sizeof(hci_read_local_features_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_READ_LOCAL_FEATURES:");
    LOG(USB_HLE_LOG, "return:");
    LOG(USB_HLE_LOG, "  features: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",   
                            Reply.features[0], Reply.features[1], Reply.features[2],
                            Reply.features[3], Reply.features[4], Reply.features[5], 
                            Reply.features[6], Reply.features[7]);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadStoredLinkKey(u8* _Input)
{
#ifdef _DEBUG
	// command parameters
    hci_read_stored_link_key_cp* ReadStoredLinkKey = (hci_read_stored_link_key_cp*)_Input;    
#endif
	// reply
    hci_read_stored_link_key_rp Reply;    
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_READ_STORED_LINK_KEY, &Reply, sizeof(hci_read_stored_link_key_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_READ_STORED_LINK_KEY:");
    LOG(USB_HLE_LOG, "input:");
    LOG(USB_HLE_LOG, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", 
        ReadStoredLinkKey->bdaddr.b[0], ReadStoredLinkKey->bdaddr.b[1], ReadStoredLinkKey->bdaddr.b[2],
        ReadStoredLinkKey->bdaddr.b[3], ReadStoredLinkKey->bdaddr.b[4], ReadStoredLinkKey->bdaddr.b[5]);
    LOG(USB_HLE_LOG, "  read_all_ %i", ReadStoredLinkKey->read_all);
    LOG(USB_HLE_LOG, "return:");
    LOG(USB_HLE_LOG, "  no idea what i should answer :)");
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteUnitClass(u8* _Input)
{
    // command parameters
    hci_write_unit_class_cp* pWriteUnitClass = (hci_write_unit_class_cp*)_Input;
    m_ClassOfDevice[0] = pWriteUnitClass->uclass[0];
    m_ClassOfDevice[1] = pWriteUnitClass->uclass[1];
    m_ClassOfDevice[2] = pWriteUnitClass->uclass[2];

    // reply
    hci_write_unit_class_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_WRITE_UNIT_CLASS, &Reply, sizeof(hci_write_unit_class_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_WRITE_UNIT_CLASS:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  COD[0]: 0x%02x", pWriteUnitClass->uclass[0]);
    LOG(USB_HLE_LOG, "  COD[1]: 0x%02x", pWriteUnitClass->uclass[1]);
    LOG(USB_HLE_LOG, "  COD[2]: 0x%02x", pWriteUnitClass->uclass[2]);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteLocalName(u8* _Input)
{
    // command parameters
    hci_write_local_name_cp* pWriteLocalName = (hci_write_local_name_cp*)_Input;
    memcpy(m_LocalName, pWriteLocalName->name, HCI_UNIT_NAME_SIZE);

    // reply
    hci_write_local_name_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_WRITE_LOCAL_NAME, &Reply, sizeof(hci_write_local_name_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_WRITE_LOCAL_NAME:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  local_name: %s", pWriteLocalName->name);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWritePinType(u8* _Input)
{
    // command parameters
    hci_write_pin_type_cp* pWritePinType = (hci_write_pin_type_cp*)_Input;
    m_PINType =  pWritePinType->pin_type;

    // reply
    hci_write_pin_type_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_WRITE_PIN_TYPE, &Reply, sizeof(hci_write_pin_type_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_WRITE_PIN_TYPE:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  pin_type: %x", pWritePinType->pin_type);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandHostBufferSize(u8* _Input)
{
    // command parameters
    hci_host_buffer_size_cp* pHostBufferSize = (hci_host_buffer_size_cp*)_Input;
    Host_max_acl_size = pHostBufferSize->max_acl_size;
    Host_max_sco_size = pHostBufferSize->max_sco_size;
    Host_num_acl_pkts = pHostBufferSize->num_acl_pkts;
    Host_num_sco_pkts = pHostBufferSize->num_sco_pkts;

    // reply
    hci_host_buffer_size_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_HOST_BUFFER_SIZE, &Reply, sizeof(hci_host_buffer_size_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_HOST_BUFFER_SIZE:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  max_acl_size: %i", pHostBufferSize->max_acl_size);
    LOG(USB_HLE_LOG, "  max_sco_size: %i", pHostBufferSize->max_sco_size);
    LOG(USB_HLE_LOG, "  num_acl_pkts: %i", pHostBufferSize->num_acl_pkts);
    LOG(USB_HLE_LOG, "  num_sco_pkts: %i", pHostBufferSize->num_sco_pkts);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWritePageTimeOut(u8* _Input)
{
#ifdef _DEBUG
    // command parameters
    hci_write_page_timeout_cp* pWritePageTimeOut = (hci_write_page_timeout_cp*)_Input;
#endif
    // reply
    hci_host_buffer_size_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_WRITE_PAGE_TIMEOUT, &Reply, sizeof(hci_host_buffer_size_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_WRITE_PAGE_TIMEOUT:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  timeout: %i", pWritePageTimeOut->timeout);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteScanEnable(u8* _Input)
{
    // command parameters
    hci_write_scan_enable_cp* pWriteScanEnable = (hci_write_scan_enable_cp*)_Input;
    scan_enable = pWriteScanEnable->scan_enable;

    // reply
    hci_write_scan_enable_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_WRITE_SCAN_ENABLE, &Reply, sizeof(hci_write_scan_enable_rp));

#ifdef _DEBUG
    static char Scanning[][128] =
    {
        { "HCI_NO_SCAN_ENABLE"},
        { "HCI_INQUIRY_SCAN_ENABLE"},
        { "HCI_PAGE_SCAN_ENABLE"},
        { "HCI_INQUIRY_AND_PAGE_SCAN_ENABLE"},
    };
#endif
    LOG(USB_HLE_LOG, "Command: HCI_CMD_WRITE_SCAN_ENABLE:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  scan_enable: %s", Scanning[pWriteScanEnable->scan_enable]);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteInquiryMode(u8* _Input)
{
#ifdef _DEBUG
    // command parameters
    hci_write_inquiry_mode_cp* pInquiryMode = (hci_write_inquiry_mode_cp*)_Input;
#endif
    // reply
    hci_write_inquiry_mode_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_MODE, &Reply, sizeof(hci_write_inquiry_mode_rp));

#ifdef _DEBUG
    static char InquiryMode[][128] =
    {
        { "Standard Inquiry Result event format (default)" },
        { "Inquiry Result format with RSSI" },
        { "Inquiry Result with RSSI format or Extended Inquiry Result format" }
    };
#endif
    LOG(USB_HLE_LOG, "Command: HCI_CMD_WRITE_INQUIRY_MODE:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  mode: %s", InquiryMode[pInquiryMode->mode]);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWritePageScanType(u8* _Input)
{
#ifdef _DEBUG
    // command parameters
    hci_write_page_scan_type_cp* pWritePageScanType = (hci_write_page_scan_type_cp*)_Input;
#endif
    // reply
    hci_write_page_scan_type_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_WRITE_PAGE_SCAN_TYPE, &Reply, sizeof(hci_write_page_scan_type_rp));

#ifdef _DEBUG
    static char PageScanType[][128] =
    {
        { "Mandatory: Standard Scan (default)" },
        { "Optional: Interlaced Scan" }
    };
#endif

    LOG(USB_HLE_LOG, "Command: HCI_CMD_WRITE_PAGE_SCAN_TYPE:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  type: %s", PageScanType[pWritePageScanType->type]);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandSetEventFilter(u8* _Input)
{
    // command parameters
    hci_set_event_filter_cp* pSetEventFilter = (hci_set_event_filter_cp*)_Input;
    filter_type = pSetEventFilter->filter_type;
    filter_condition_type = pSetEventFilter->filter_condition_type;

    // reply
    hci_set_event_filter_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_SET_EVENT_FILTER, &Reply, sizeof(hci_set_event_filter_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_SET_EVENT_FILTER:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  filter_type: %i", pSetEventFilter->filter_type);
    LOG(USB_HLE_LOG, "  filter_condition_type: %i", pSetEventFilter->filter_condition_type);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandInquiry(u8* _Input)
{
    // command parameters
    hci_inquiry_cp* pInquiry = (hci_inquiry_cp*)_Input;
    u8 lap[HCI_LAP_SIZE];
    
    memcpy(lap, pInquiry->lap, HCI_LAP_SIZE);
#ifdef _DEBUG
    u8 inquiry_length = pInquiry->inquiry_length;
    u8 num_responses = pInquiry->num_responses;
#endif
    _dbg_assert_msg_(USB_HLE_LOG, m_State == STATE_NONE, "m_State != NONE");
    m_State = STATE_INQUIRY_RESPONSE;
    SendEventCommandStatus(HCI_CMD_INQUIRY);

    LOG(USB_HLE_LOG, "Command: HCI_CMD_INQUIRY:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  LAP[0]: 0x%02x", pInquiry->lap[0]);
    LOG(USB_HLE_LOG, "  LAP[1]: 0x%02x", pInquiry->lap[1]);
    LOG(USB_HLE_LOG, "  LAP[2]: 0x%02x", pInquiry->lap[2]);
    LOG(USB_HLE_LOG, "  inquiry_length: %i (N x 1.28) sec", pInquiry->inquiry_length);
    LOG(USB_HLE_LOG, "  num_responses: %i (N x 1.28) sec", pInquiry->num_responses);        
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandWriteInquiryScanType(u8* _Input)
{
#ifdef _DEBUG
    // command parameters
    hci_write_inquiry_scan_type_cp* pSetEventFilter = (hci_write_inquiry_scan_type_cp*)_Input;
#endif
    // reply
    hci_write_inquiry_scan_type_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_WRITE_INQUIRY_SCAN_TYPE, &Reply, sizeof(hci_write_inquiry_scan_type_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_WRITE_INQUIRY_SCAN_TYPE:");
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  type: %i", pSetEventFilter->type);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandVendorSpecific_FC4F(u8* _Input, u32 _Size)
{
    // callstack...
    // BTM_VendorSpecificCommad()
    // WUDiRemovePatch()
    // WUDiAppendRuntimePatch()
    // WUDiGetFirmwareVersion()
    // WUDiStackSetupComplete()

    // reply
    hci_status_rp Reply;
	Reply.status = 0x00;

	SendEventCommandComplete(0xFC4F, &Reply, sizeof(hci_status_rp));

    LOG(USB_HLE_LOG, "Command: CommandVendorSpecific_FC4F:");
	LOG(USB_HLE_LOG, "input (size 0x%x):", _Size);
	for (u32 i=0; i<_Size; i++)
		LOG(USB_HLE_LOG, "  Data: 0x%02x", _Input[i]);
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  callstack WUDiRemovePatch");
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandVendorSpecific_FC4C(u8* _Input, u32 _Size)
{
    // reply
    hci_status_rp Reply;
	Reply.status = 0x00;

	SendEventCommandComplete(0xFC4C, &Reply, sizeof(hci_status_rp));

    LOG(USB_HLE_LOG, "Command: CommandVendorSpecific_FC4C:");
	LOG(USB_HLE_LOG, "input (size 0x%x):", _Size);
	for (u32 i=0; i<_Size; i++)
		LOG(USB_HLE_LOG, "  Data: 0x%02x", _Input[i]);
    LOG(USB_HLE_LOG, "write:");
    LOG(USB_HLE_LOG, "  perhaps append patch?");
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandInquiryCancel(u8* _Input)
{
    // reply
    hci_inquiry_cancel_rp Reply;
    Reply.status = 0x00;

	SendEventCommandComplete(HCI_CMD_INQUIRY_CANCEL, &Reply, sizeof(hci_inquiry_cancel_rp));

    LOG(USB_HLE_LOG, "Command: HCI_CMD_INQUIRY_CANCEL");
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandRemoteNameReq(u8* _Input)
{
	// command parameters
	hci_remote_name_req_cp* pRemoteNameReq = (hci_remote_name_req_cp*)_Input;

    _dbg_assert_msg_(USB_HLE_LOG, m_State == STATE_NONE, "m_State != NONE");
    SendEventCommandStatus(HCI_CMD_REMOTE_NAME_REQ);
	m_StateTempBD = pRemoteNameReq->bdaddr;
	m_State = STATE_REMOTE_NAME_REQ;

	LOG(USB_HLE_LOG, "Command: HCI_CMD_REMOTE_NAME_REQ");
	LOG(USB_HLE_LOG, "Input:");
	LOG(USB_HLE_LOG, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", 
		pRemoteNameReq->bdaddr.b[0], pRemoteNameReq->bdaddr.b[1], pRemoteNameReq->bdaddr.b[2],
		pRemoteNameReq->bdaddr.b[3], pRemoteNameReq->bdaddr.b[4], pRemoteNameReq->bdaddr.b[5]);
	LOG(USB_HLE_LOG, "  page_scan_rep_mode: %i", pRemoteNameReq->page_scan_rep_mode);
	LOG(USB_HLE_LOG, "  page_scan_mode: %i", pRemoteNameReq->page_scan_mode);
	LOG(USB_HLE_LOG, "  clock_offset: %i", pRemoteNameReq->clock_offset)
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandCreateCon(u8* _Input)
{
	// command parameters
	hci_create_con_cp* pCreateCon = (hci_create_con_cp*)_Input;

    _dbg_assert_msg_(USB_HLE_LOG, m_State == STATE_NONE, "m_State != NONE");
    m_State = STATE_CONNECTION_COMPLETE_EVENT;
	m_StateTempBD = pCreateCon->bdaddr;
    SendEventCommandStatus(HCI_CMD_CREATE_CON);
	
	LOG(USB_HLE_LOG, "Command: HCI_CMD_CREATE_CON");
	LOG(USB_HLE_LOG, "Input:");
	LOG(USB_HLE_LOG, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", 
		pCreateCon->bdaddr.b[0], pCreateCon->bdaddr.b[1], pCreateCon->bdaddr.b[2],
		pCreateCon->bdaddr.b[3], pCreateCon->bdaddr.b[4], pCreateCon->bdaddr.b[5]);

	LOG(USB_HLE_LOG, "  pkt_type: %i", pCreateCon->pkt_type);
	LOG(USB_HLE_LOG, "  page_scan_rep_mode: %i", pCreateCon->page_scan_rep_mode);
	LOG(USB_HLE_LOG, "  page_scan_mode: %i", pCreateCon->page_scan_mode);
	LOG(USB_HLE_LOG, "  clock_offset: %i", pCreateCon->clock_offset);
	LOG(USB_HLE_LOG, "  accept_role_switch: %i", pCreateCon->accept_role_switch);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandAcceptCon(u8* _Input)
{
#ifdef _DEBUG
	// command parameters
	hci_accept_con_cp* pAcceptCon = (hci_accept_con_cp*)_Input;
#endif
	LOG(USB_HLE_LOG, "Command: HCI_CMD_ACCEPT_CON");
	LOG(USB_HLE_LOG, "Input:");
	LOG(USB_HLE_LOG, "  bd: %02x:%02x:%02x:%02x:%02x:%02x", 
		pAcceptCon->bdaddr.b[0], pAcceptCon->bdaddr.b[1], pAcceptCon->bdaddr.b[2],
		pAcceptCon->bdaddr.b[3], pAcceptCon->bdaddr.b[4], pAcceptCon->bdaddr.b[5]);
	LOG(USB_HLE_LOG, " role: %i", pAcceptCon->role);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadClockOffset(u8* _Input)
{
    // command parameters
    hci_read_clock_offset_cp* pReadClockOffset = (hci_read_clock_offset_cp*)_Input;

    _dbg_assert_msg_(USB_HLE_LOG, m_State == STATE_NONE, "m_State != NONE");    
    m_State = STATE_READ_CLOCK_OFFSET;
	m_StateTempConnectionHandle = pReadClockOffset->con_handle;
    SendEventCommandStatus(HCI_CMD_READ_CLOCK_OFFSET);
    
    LOG(USB_HLE_LOG, "Command: HCI_CMD_READ_CLOCK_OFFSET");
    LOG(USB_HLE_LOG, "Input:");
    LOG(USB_HLE_LOG, "  ConnectionHandle: 0x%02x", pReadClockOffset->con_handle);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadRemoteVerInfo(u8* _Input)
{
    // command parameters
    hci_read_remote_ver_info_cp* pReadRemoteVerInfo = (hci_read_remote_ver_info_cp*)_Input;

    _dbg_assert_msg_(USB_HLE_LOG, m_State == STATE_NONE, "m_State != NONE (%i)", m_State);
    m_State = STATE_READ_REMOTE_VER_INFO;
	m_StateTempConnectionHandle = pReadRemoteVerInfo->con_handle;
    SendEventCommandStatus(HCI_CMD_READ_REMOTE_VER_INFO);

    LOG(USB_HLE_LOG, "Command: HCI_CMD_READ_REMOTE_VER_INFO");
    LOG(USB_HLE_LOG, "Input:");
    LOG(USB_HLE_LOG, "  ConnectionHandle: 0x%02x", pReadRemoteVerInfo->con_handle);
}

void CWII_IPC_HLE_Device_usb_oh1_57e_305::CommandReadRemoteFeatures(u8* _Input)
{
    // command parameters
    hci_read_remote_features_cp* pReadRemoteFeatures = (hci_read_remote_features_cp*)_Input;

    _dbg_assert_msg_(USB_HLE_LOG, m_State == STATE_NONE, "m_State != NONE");
    m_State = STATE_READ_REMOTE_FEATURES;
	m_StateTempConnectionHandle = pReadRemoteFeatures->con_handle;
    SendEventCommandStatus(HCI_CMD_READ_REMOTE_FEATURES);

    LOG(USB_HLE_LOG, "Command: HCI_CMD_READ_REMOTE_FEATURES");
    LOG(USB_HLE_LOG, "Input:");
    LOG(USB_HLE_LOG, "  ConnectionHandle: 0x%02x", pReadRemoteFeatures->con_handle);
}

CWII_IPC_HLE_WiiMote* CWII_IPC_HLE_Device_usb_oh1_57e_305::AccessWiiMote(const bdaddr_t& _rAddr)
{
	for (size_t i=0; i<m_WiiMotes.size(); i++)
	{
		const bdaddr_t& BD = m_WiiMotes[i].GetBD();
		if ((_rAddr.b[0] == BD.b[0]) &&
			(_rAddr.b[1] == BD.b[1]) &&
			(_rAddr.b[2] == BD.b[2]) &&
			(_rAddr.b[3] == BD.b[3]) &&
			(_rAddr.b[4] == BD.b[4]) &&
			(_rAddr.b[5] == BD.b[5]))
			return &m_WiiMotes[i];
	}
	return NULL;
}

CWII_IPC_HLE_WiiMote* CWII_IPC_HLE_Device_usb_oh1_57e_305::AccessWiiMote(u16 _ConnectionHandle)
{
	for (size_t i=0; i<m_WiiMotes.size(); i++)
	{
		if (m_WiiMotes[i].GetConnectionHandle() == _ConnectionHandle)
			return &m_WiiMotes[i];
	}
	return NULL;
}
void CWII_IPC_HLE_Device_usb_oh1_57e_305::ClearBD(bdaddr_t& _rAddr)
{
	memset(_rAddr.b, 0, BLUETOOTH_BDADDR_SIZE);
}
