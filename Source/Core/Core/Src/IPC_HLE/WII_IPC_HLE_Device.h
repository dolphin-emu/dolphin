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
#ifndef _WII_IPC_HLE_DEVICE_H_
#define _WII_IPC_HLE_DEVICE_H_

#include <string>
#include <queue>
#include "../HW/Memmap.h"
#include "../HW/CPU.h"

class IWII_IPC_HLE_Device
{
public:

	IWII_IPC_HLE_Device(u32 _DeviceID, const std::string& _rName) :
        m_Name(_rName),
		m_DeviceID(_DeviceID)
	{}

	virtual ~IWII_IPC_HLE_Device()
	{}

	const std::string& GetDeviceName() const { return m_Name; } 
    u32 GetDeviceID() const { return m_DeviceID; } 

    virtual bool Open(u32 _CommandAddress)  { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Open()", m_Name.c_str()); return true; }
    virtual bool Close(u32 _CommandAddress)  { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Close()", m_Name.c_str()); return true; }
    virtual bool Seek(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Seek()", m_Name.c_str()); return true; }
	virtual bool Read(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Read()", m_Name.c_str()); return true; }
	virtual bool Write(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Write()", m_Name.c_str()); return true; }
	virtual bool IOCtl(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run IOCtl()", m_Name.c_str()); return true; }
	virtual bool IOCtlV(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run IOCtlV()", m_Name.c_str()); return true; }

	virtual u32 Update() { return 0; }

protected:

    struct SIOCtlVBuffer
    {
        SIOCtlVBuffer(u32 _Address)
            : m_Address(_Address)
        {
            Parameter = Memory::Read_U32(m_Address + 0x0C);
            NumberInBuffer = Memory::Read_U32(m_Address + 0x10);
            NumberPayloadBuffer = Memory::Read_U32(m_Address + 0x14);
            BufferVector = Memory::Read_U32(m_Address + 0x18);
            BufferSize = Memory::Read_U32(m_Address + 0x1C);

            u32 BufferVectorOffset = BufferVector;
            for (u32 i=0; i<NumberInBuffer; i++)
            {
                SBuffer Buffer;
                Buffer.m_Address = Memory::Read_U32(BufferVectorOffset); BufferVectorOffset += 4;
                Buffer.m_Size = Memory::Read_U32(BufferVectorOffset); BufferVectorOffset += 4;
                InBuffer.push_back(Buffer);
            }
            for (u32 i=0; i<NumberPayloadBuffer; i++)
            {
                SBuffer Buffer;
                Buffer.m_Address = Memory::Read_U32(BufferVectorOffset); BufferVectorOffset += 4;
                Buffer.m_Size = Memory::Read_U32(BufferVectorOffset); BufferVectorOffset += 4;
                PayloadBuffer.push_back(Buffer);
            }            
        }

 		// STATE_TO_SAVE
        const u32 m_Address;

        u32 Parameter;
        u32 NumberInBuffer;
        u32 NumberPayloadBuffer;
        u32 BufferVector;
        u32 BufferSize;


        struct SBuffer { u32 m_Address, m_Size; };
        std::vector<SBuffer> InBuffer;
        std::vector<SBuffer> PayloadBuffer;
    };

    void DumpCommands(u32 _CommandAddress, size_t _NumberOfCommands = 8)
    {
        LOG(WII_IPC_HLE, "  CommandDump of %s", GetDeviceName().c_str());
        for (u32 i=0; i<_NumberOfCommands; i++)
        {            
            LOG(WII_IPC_HLE, "    Command%02i: 0x%08x", i, Memory::Read_U32(_CommandAddress + i*4));	
        }
    }

    void DumpAsync( u32 BufferVector, u32 _CommandAddress, u32 NumberInBuffer, u32 NumberOutBuffer )
    {
		LOG(WII_IPC_HLE, "======= DumpAsync ======");
        // write return value
        u32 BufferOffset = BufferVector;
        Memory::Write_U32(1, _CommandAddress + 0x4);

        for (u32 i=0; i<NumberInBuffer; i++)
        {            
            u32 InBuffer        = Memory::Read_U32(BufferOffset); BufferOffset += 4;
            u32 InBufferSize    = Memory::Read_U32(BufferOffset); BufferOffset += 4;

            LOG(WII_IPC_HLE, "%s - IOCtlV InBuffer[%i]:", GetDeviceName().c_str(), i);

            std::string Temp;
            for (u32 j=0; j<InBufferSize; j++)
            {
                char Buffer[128];
                sprintf(Buffer, "%02x ", Memory::Read_U8(InBuffer+j));
                Temp.append(Buffer);
            }

            LOG(WII_IPC_HLE, "    Buffer: %s", Temp.c_str());
        }


        for (u32 i=0; i<NumberOutBuffer; i++)
        {   
#ifdef _DEBUG
            u32 OutBuffer        = Memory::Read_U32(BufferOffset); BufferOffset += 4;
            u32 OutBufferSize    = Memory::Read_U32(BufferOffset); BufferOffset += 4;
#endif

            Memory::Write_U32(1, _CommandAddress + 0x4);

            LOG(WII_IPC_HLE, "%s - IOCtlV OutBuffer[%i]:", GetDeviceName().c_str(), i);
            LOG(WII_IPC_HLE, "    OutBuffer: 0x%08x (0x%x):", OutBuffer, OutBufferSize);
            // DumpCommands(OutBuffer, OutBufferSize);
        }
    }

private:

	// STATE_TO_SAVE
	std::string m_Name;
    u32 m_DeviceID;

};

#endif

