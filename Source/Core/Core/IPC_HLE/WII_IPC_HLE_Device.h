// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <queue>
#include <string>
#include <vector>

#include "Common/ChunkFile.h"
#include "Common/StringUtil.h"
#include "Core/HW/Memmap.h"
#include "Core/IPC_HLE/WII_IPC_HLE.h"

#define FS_SUCCESS      (u32)0      // Success
#define FS_EACCES       (u32)-1     // Permission denied
#define FS_EEXIST       (u32)-2     // File exists
#define FS_EINVAL       (u32)-4     // Invalid argument Invalid FD
#define FS_ENOENT       (u32)-6     // File not found
#define FS_EBUSY        (u32)-8     // Resource busy
#define FS_EIO          (u32)-12    // Returned on ECC error
#define FS_ENOMEM       (u32)-22    // Alloc failed during request
#define FS_EFATAL       (u32)-101   // Fatal error
#define FS_EACCESS      (u32)-102   // Permission denied
#define FS_ECORRUPT     (u32)-103   // returned for "corrupted" NAND
#define FS_EEXIST2      (u32)-105   // File exists
#define FS_ENOENT2      (u32)-106   // File not found
#define FS_ENFILE       (u32)-107   // Too many fds open
#define FS_EFBIG        (u32)-108   // Max block count reached?
#define FS_EFDEXHAUSTED (u32)-109   // Too many fds open
#define FS_ENAMELEN     (u32)-110   // Pathname is too long
#define FS_EFDOPEN      (u32)-111   // FD is already open
#define FS_EIO2         (u32)-114   // Returned on ECC error
#define FS_ENOTEMPTY    (u32)-115   // Directory not empty
#define FS_EDIRDEPTH    (u32)-116   // Max directory depth exceeded
#define FS_EBUSY2       (u32)-118   // Resource busy
//#define FS_EFATAL       (u32)-119   // Fatal error not used by IOS as fatal ERROR
#define FS_EESEXHAUSTED (u32)-1016  // Max of 2 ES handles at a time

// A struct for IOS ioctlv calls
struct SIOCtlVBuffer
{
	explicit SIOCtlVBuffer(u32 address);

	const u32 m_Address;

	u32 Parameter;
	u32 NumberInBuffer;
	u32 NumberPayloadBuffer;
	u32 BufferVector;

	struct SBuffer { u32 m_Address, m_Size; };
	std::vector<SBuffer> InBuffer;
	std::vector<SBuffer> PayloadBuffer;
};

class IWII_IPC_HLE_Device
{
public:
	IWII_IPC_HLE_Device(u32 device_id, const std::string& name, bool hardware = true);
	virtual ~IWII_IPC_HLE_Device();

	virtual void DoState(PointerWrap& p);
	void DoStateShared(PointerWrap& p);

	const std::string& GetDeviceName() const { return m_Name; }
	u32 GetDeviceID() const { return m_DeviceID; }

	virtual IPCCommandResult Open(u32 command_address, u32 mode);
	virtual IPCCommandResult Close(u32 command_address, bool force = false);
	virtual IPCCommandResult Seek(u32 command_address);
	virtual IPCCommandResult Read(u32 command_address);
	virtual IPCCommandResult Write(u32 command_address);
	virtual IPCCommandResult IOCtl(u32 command_address);
	virtual IPCCommandResult IOCtlV(u32 command_address);

	virtual u32 Update() { return 0; }

	virtual bool IsHardware() { return m_Hardware; }
	virtual bool IsOpened() { return m_Active; }

	// Returns an IPCCommandResult for a reply that takes 250 us (arbitrarily chosen value)
	static IPCCommandResult GetDefaultReply();
	// Returns an IPCCommandResult with no reply. Useful for async commands that will generate a reply later
	static IPCCommandResult GetNoReply();

	std::string m_Name;
protected:
	// Write out the IPC struct from _CommandAddress to _NumberOfCommands numbers
	// of 4 byte commands.
	void DumpCommands(u32 command_address, size_t number_of_commands = 8,
	                  LogTypes::LOG_TYPE type = LogTypes::WII_IPC_HLE,
	                  LogTypes::LOG_LEVELS verbosity = LogTypes::LDEBUG) const;

	void DumpAsync(u32 buffer_vector, u32 number_in_buffer, u32 number_out_buffer,
	               LogTypes::LOG_TYPE type = LogTypes::WII_IPC_HLE,
	               LogTypes::LOG_LEVELS verbosity = LogTypes::LDEBUG) const;

	// STATE_TO_SAVE
	u32 m_DeviceID;
	bool m_Hardware;
	bool m_Active;
};

