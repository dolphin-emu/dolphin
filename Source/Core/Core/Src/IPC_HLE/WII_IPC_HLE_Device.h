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
#ifndef _WII_IPC_HLE_DEVICE_H_
#define _WII_IPC_HLE_DEVICE_H_

#include <string>
#include <queue>
#include "../HW/Memmap.h"
#include "../HW/CPU.h"

class PointerWrap;

class IWII_IPC_HLE_Device
{
public:

	IWII_IPC_HLE_Device(u32 _DeviceID, const std::string& _rName, bool _Hardware = true) :
        m_Name(_rName),
		m_DeviceID(_DeviceID),
		m_Hardware(_Hardware),
		m_Active(false)
	{}

	virtual ~IWII_IPC_HLE_Device()
	{}

	virtual void DoState(PointerWrap &p)
	{}

	const std::string& GetDeviceName() const { return m_Name; } 
    u32 GetDeviceID() const { return m_DeviceID; } 

    virtual bool Open(u32 _CommandAddress, u32 _Mode)  { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Open()", m_Name.c_str()); m_Active = true; return true; }
    virtual bool Close(u32 _CommandAddress, bool _bForce = false)  { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Close()", m_Name.c_str()); m_Active = false; return true; }
    virtual bool Seek(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Seek()", m_Name.c_str()); return true; }
	virtual bool Read(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Read()", m_Name.c_str()); return true; }
	virtual bool Write(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run Write()", m_Name.c_str()); return true; }
	virtual bool IOCtl(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run IOCtl()", m_Name.c_str()); return true; }
	virtual bool IOCtlV(u32 _CommandAddress) { _dbg_assert_msg_(WII_IPC_HLE, 0, "%s is not able to run IOCtlV()", m_Name.c_str()); return true; }

	virtual u32 Update() { return 0; }

	virtual bool ReturnFileHandle() { return false; }

	virtual bool IsHardware() { return m_Hardware; }
	virtual bool IsOpened() { return m_Active; }

protected:

	// STATE_TO_SAVE
	std::string m_Name;
	u32 m_DeviceID;
	bool m_Hardware;
	bool m_Active;

	// ===================================================
	/* A struct for IOS ioctlv calls */
	// ----------------
    struct SIOCtlVBuffer
    {
        SIOCtlVBuffer(u32 _Address)
            : m_Address(_Address)
        {
			/* These are the Ioctlv parameters in the IOS communication. The BufferVector
			   is a memory address offset at where the in and out buffer addresses are
			   stored. */
            Parameter = Memory::Read_U32(m_Address + 0x0C); // command 3, arg0
            NumberInBuffer = Memory::Read_U32(m_Address + 0x10); // 4, arg1
            NumberPayloadBuffer = Memory::Read_U32(m_Address + 0x14); // 5, arg2
            BufferVector = Memory::Read_U32(m_Address + 0x18); // 6, arg3

			// The start of the out buffer
            u32 BufferVectorOffset = BufferVector;

			//if(Parameter = 0x1d) PanicAlert("%i: %i", Parameter, NumberInBuffer);

			// Write the address and size for all in messages
            for (u32 i = 0; i < NumberInBuffer; i++)
            {
                SBuffer Buffer;
                Buffer.m_Address = Memory::Read_U32(BufferVectorOffset);

				// Restore cached address, mauled by emulator's ioctl functions.
				Memory::Write_U32(Buffer.m_Address | 0x80000000, BufferVectorOffset);
				BufferVectorOffset += 4;

                Buffer.m_Size = Memory::Read_U32(BufferVectorOffset);
				BufferVectorOffset += 4;
				DEBUG_LOG(WII_IPC_HLE, "SIOCtlVBuffer in%i: 0x%08x, 0x%x",
						  i, Buffer.m_Address, Buffer.m_Size);
                InBuffer.push_back(Buffer);
            }

			// Write the address and size for all out or in-out messages
            for (u32 i = 0; i < NumberPayloadBuffer; i++)
            {
                SBuffer Buffer;
                Buffer.m_Address = Memory::Read_U32(BufferVectorOffset);

				Memory::Write_U32(Buffer.m_Address | 0x80000000, BufferVectorOffset);
				BufferVectorOffset += 4;

                Buffer.m_Size = Memory::Read_U32(BufferVectorOffset);
				BufferVectorOffset += 4;
				DEBUG_LOG(WII_IPC_HLE, "SIOCtlVBuffer io%i: 0x%08x, 0x%x",
						  i, Buffer.m_Address, Buffer.m_Size);
                PayloadBuffer.push_back(Buffer);
            }
        }

        const u32 m_Address;

        u32 Parameter;
        u32 NumberInBuffer;
        u32 NumberPayloadBuffer;
        u32 BufferVector;

        struct SBuffer { u32 m_Address, m_Size; };
        std::vector<SBuffer> InBuffer;
        std::vector<SBuffer> PayloadBuffer;
    };


	// ===================================================
	/* Write out the IPC struct from _CommandAddress to _NumberOfCommands numbers
	   of 4 byte commands. */
	// ----------------
    void DumpCommands(u32 _CommandAddress, size_t _NumberOfCommands = 8,
		LogTypes::LOG_TYPE LogType = LogTypes::WII_IPC_HLE,
		LogTypes::LOG_LEVELS Verbosity = LogTypes::LDEBUG)
    {
		GENERIC_LOG(LogType, Verbosity, "CommandDump of %s", 
					GetDeviceName().c_str());
		for (u32 i = 0; i < _NumberOfCommands; i++)
			{            
				GENERIC_LOG(LogType, Verbosity, "    Command%02i: 0x%08x", i,
							Memory::Read_U32(_CommandAddress + i*4));	
			}
    }

	
    void DumpAsync(u32 BufferVector, u32 _CommandAddress, u32 NumberInBuffer, u32 NumberOutBuffer
		,LogTypes::LOG_TYPE LogType = LogTypes::WII_IPC_HLE,
		LogTypes::LOG_LEVELS Verbosity = LogTypes::LDEBUG)
    {
		GENERIC_LOG(LogType, Verbosity, "======= DumpAsync ======");

        u32 BufferOffset = BufferVector;

        for (u32 i = 0; i < NumberInBuffer; i++)
        {            
            u32 InBuffer        = Memory::Read_U32(BufferOffset); BufferOffset += 4;
            u32 InBufferSize    = Memory::Read_U32(BufferOffset); BufferOffset += 4;

            GENERIC_LOG(LogType, LogTypes::LINFO, "%s - IOCtlV InBuffer[%i]:", GetDeviceName().c_str(), i);

            std::string Temp;
            for (u32 j = 0; j < InBufferSize; j++)
            {
                char Buffer[128];
                sprintf(Buffer, "%02x ", Memory::Read_U8(InBuffer+j));
                Temp.append(Buffer);
            }

            GENERIC_LOG(LogType, LogTypes::LDEBUG, "    Buffer: %s", Temp.c_str());
        }


        for (u32 i = 0; i < NumberOutBuffer; i++)
        {
			u32 OutBuffer        = Memory::Read_U32(BufferOffset); BufferOffset += 4;
			u32 OutBufferSize    = Memory::Read_U32(BufferOffset); BufferOffset += 4;

            GENERIC_LOG(LogType, LogTypes::LINFO, "%s - IOCtlV OutBuffer[%i]:", GetDeviceName().c_str(), i);
            GENERIC_LOG(LogType, LogTypes::LINFO, "    OutBuffer: 0x%08x (0x%x):", OutBuffer, OutBufferSize);

			#if defined(MAX_LOGLEVEL) && MAX_LOGLEVEL >= INFO_LEVEL
			DumpCommands(OutBuffer, OutBufferSize, LogType, Verbosity);
			#endif
       }
    }

};

#endif

