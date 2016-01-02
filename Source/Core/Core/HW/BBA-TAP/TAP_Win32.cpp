// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Assert.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"
#include "Core/HW/EXI_Device.h"
#include "Core/HW/EXI_DeviceEthernet.h"
#include "Core/HW/BBA-TAP/TAP_Win32.h"

namespace Win32TAPHelper
{

bool IsTAPDevice(const TCHAR *guid)
{
	HKEY netcard_key;
	LONG status;
	DWORD len;
	int i = 0;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, ADAPTER_KEY, 0, KEY_READ, &netcard_key);

	if (status != ERROR_SUCCESS)
		return false;

	for (;;)
	{
		TCHAR enum_name[256];
		TCHAR unit_string[256];
		HKEY unit_key;
		TCHAR component_id_string[] = _T("ComponentId");
		TCHAR component_id[256];
		TCHAR net_cfg_instance_id_string[] = _T("NetCfgInstanceId");
		TCHAR net_cfg_instance_id[256];
		DWORD data_type;

		len = sizeof(enum_name);
		status = RegEnumKeyEx(netcard_key, i, enum_name, &len, nullptr, nullptr, nullptr, nullptr);

		if (status == ERROR_NO_MORE_ITEMS)
			break;
		else if (status != ERROR_SUCCESS)
			return false;

		_sntprintf(unit_string, sizeof(unit_string), _T("%s\\%s"), ADAPTER_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, unit_string, 0, KEY_READ, &unit_key);

		if (status != ERROR_SUCCESS)
		{
			return false;
		}
		else
		{
			len = sizeof(component_id);
			status = RegQueryValueEx(unit_key, component_id_string, nullptr,
				&data_type, (LPBYTE)component_id, &len);

			if (!(status != ERROR_SUCCESS || data_type != REG_SZ))
			{
				len = sizeof(net_cfg_instance_id);
				status = RegQueryValueEx(unit_key, net_cfg_instance_id_string, nullptr,
					&data_type, (LPBYTE)net_cfg_instance_id, &len);

				if (status == ERROR_SUCCESS && data_type == REG_SZ)
				{
					if (!_tcscmp(component_id, TAP_COMPONENT_ID) &&
						!_tcscmp(net_cfg_instance_id, guid))
					{
						RegCloseKey(unit_key);
						RegCloseKey(netcard_key);
						return true;
					}
				}
			}
			RegCloseKey(unit_key);
		}
		++i;
	}

	RegCloseKey(netcard_key);
	return false;
}

bool GetGUIDs(std::vector<std::basic_string<TCHAR>>& guids)
{
	LONG status;
	HKEY control_net_key;
	DWORD len;
	DWORD cSubKeys = 0;

	status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, NETWORK_CONNECTIONS_KEY, 0, KEY_READ | KEY_QUERY_VALUE, &control_net_key);

	if (status != ERROR_SUCCESS)
		return false;

	status = RegQueryInfoKey(control_net_key, nullptr, nullptr, nullptr, &cSubKeys, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

	if (status != ERROR_SUCCESS)
		return false;

	for (DWORD i = 0; i < cSubKeys; i++)
	{
		TCHAR enum_name[256];
		TCHAR connection_string[256];
		HKEY connection_key;
		TCHAR name_data[256];
		DWORD name_type;
		const TCHAR name_string[] = _T("Name");

		len = sizeof(enum_name);
		status = RegEnumKeyEx(control_net_key, i, enum_name,
			&len, nullptr, nullptr, nullptr, nullptr);

		if (status != ERROR_SUCCESS)
			continue;

		_sntprintf(connection_string, sizeof(connection_string),
			_T("%s\\%s\\Connection"), NETWORK_CONNECTIONS_KEY, enum_name);

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, connection_string,
			0, KEY_READ, &connection_key);

		if (status == ERROR_SUCCESS)
		{
			len = sizeof(name_data);
			status = RegQueryValueEx(connection_key, name_string, nullptr,
				&name_type, (LPBYTE)name_data, &len);

			if (status != ERROR_SUCCESS || name_type != REG_SZ)
			{
				continue;
			}
			else
			{
				if (IsTAPDevice(enum_name))
				{
					guids.push_back(enum_name);
				}
			}

			RegCloseKey(connection_key);
		}
	}

	RegCloseKey(control_net_key);

	return !guids.empty();
}

bool OpenTAP(HANDLE& adapter, const std::basic_string<TCHAR>& device_guid)
{
	auto const device_path = USERMODEDEVICEDIR + device_guid + TAPSUFFIX;

	adapter = CreateFile(device_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_OVERLAPPED, nullptr);

	if (adapter == INVALID_HANDLE_VALUE)
	{
		INFO_LOG(SP1, "Failed to open TAP at %s", device_path);
		return false;
	}
	return true;
}

} // namespace Win32TAPHelper

bool CEXIETHERNET::Activate()
{
	if (IsActivated())
		return true;

	DWORD len;
	std::vector<std::basic_string<TCHAR>> device_guids;

	if (!Win32TAPHelper::GetGUIDs(device_guids))
	{
		ERROR_LOG(SP1, "Failed to find a TAP GUID");
		return false;
	}

	for (size_t i = 0; i < device_guids.size(); i++)
	{
		if (Win32TAPHelper::OpenTAP(mHAdapter, device_guids.at(i)))
		{
			INFO_LOG(SP1, "OPENED %s", device_guids.at(i).c_str());
			break;
		}
	}
	if (mHAdapter == INVALID_HANDLE_VALUE)
	{
		PanicAlert("Failed to open any TAP");
		return false;
	}

	/* get driver version info */
	ULONG info[3];
	if (DeviceIoControl(mHAdapter, TAP_IOCTL_GET_VERSION,
		&info, sizeof(info), &info, sizeof(info), &len, nullptr))
	{
		INFO_LOG(SP1, "TAP-Win32 Driver Version %d.%d %s",
			info[0], info[1], info[2] ? "(DEBUG)" : "");
	}
	if (!(info[0] > TAP_WIN32_MIN_MAJOR || (info[0] == TAP_WIN32_MIN_MAJOR && info[1] >= TAP_WIN32_MIN_MINOR)))
	{
		PanicAlertT("ERROR: This version of Dolphin requires a TAP-Win32 driver"
			" that is at least version %d.%d -- If you recently upgraded your Dolphin"
			" distribution, a reboot is probably required at this point to get"
			" Windows to see the new driver.",
			TAP_WIN32_MIN_MAJOR, TAP_WIN32_MIN_MINOR);
		return false;
	}

	/* set driver media status to 'connected' */
	ULONG status = TRUE;
	if (!DeviceIoControl(mHAdapter, TAP_IOCTL_SET_MEDIA_STATUS,
		&status, sizeof(status), &status, sizeof(status), &len, nullptr))
	{
		ERROR_LOG(SP1, "WARNING: The TAP-Win32 driver rejected a"
			"TAP_IOCTL_SET_MEDIA_STATUS DeviceIoControl call.");
		return false;
	}

	// Create IO completion port, and associate the adapter with this port.
	mHCompletionPort = CreateIoCompletionPort(mHAdapter, nullptr, reinterpret_cast<ULONG_PTR>(this), 0);
	if (!mHCompletionPort)
	{
		ERROR_LOG(SP1, "TAP_WIN32: Failed to create IO completion port (err=%x)", GetLastError());
		return false;
	}

	// Start IO thread
	return RecvInit();
}

void CEXIETHERNET::Deactivate()
{
	if (!IsActivated())
		return;

	RecvStop();

	// Queue an IO packet that drains the queue, and wait for the IO thread to exit.
	PostQueuedCompletionStatus(mHCompletionPort, 0, 0, nullptr);
	if (readThread.joinable())
		readThread.join();

	// Clean-up handles
	CloseHandle(mHAdapter);
	mHAdapter = INVALID_HANDLE_VALUE;
	CloseHandle(mHCompletionPort);
	mHCompletionPort = INVALID_HANDLE_VALUE;
}

bool CEXIETHERNET::IsActivated()
{
	return mHAdapter != INVALID_HANDLE_VALUE;
}

// This structure allows us to attach a request type to the OVERLAPPED structure.
// A virtual destructor is omitted here since the type field determines if it's a
// write packet anyway, so it will be casted before being freed.
struct AsyncIOHeader
{
	enum IOPacketType : u32
	{
		IO_PACKET_TYPE_READ,
		IO_PACKET_TYPE_WRITE
	};

	IOPacketType type;
	OVERLAPPED overlapped;
};

// The buffer size in here is chosen so that it will include most small packets
// sent by games, and saves an extra allocation in these cases, as well as
// keeping the total size of this structure under 256 bytes. If a packet is
// larger, a second buffer is stored in frame_buffer.
static const size_t SMALL_FRAME_BUFFER_SIZE = 200;
struct AsyncIOWritePacket final : public AsyncIOHeader
{
	u8 small_frame_buffer[SMALL_FRAME_BUFFER_SIZE];
	std::unique_ptr<u8[]> frame_buffer;
};

static bool QueueRead(CEXIETHERNET* self, AsyncIOHeader* hdr)
{
	// Initialize fields as a read request.
	hdr->type = AsyncIOHeader::IO_PACKET_TYPE_READ;
	ZeroMemory(&hdr->overlapped, sizeof(hdr->overlapped));

	// Queue asynchronous ReadFile. It will always return later via GetQueuedCompletionStatus.
	ReadFile(self->mHAdapter, self->mRecvBuffer, BBA_RECV_SIZE, nullptr, &hdr->overlapped);

	// Check something else did not go wrong, and the request was queued.
	DWORD err = GetLastError();
	if (err != ERROR_IO_PENDING)
	{
		ERROR_LOG(SP1, "ReadFile failed (err=0x%X)", err);
		return false;
	}

	return true;
}

static void ReadThreadHandler(CEXIETHERNET* self)
{
	// Queue an asynchronous read into the buffer in the CEXIETHERNET class.
	// Unlike writes, there is a persistent buffer for reads, since we can only have one in-flight at once. This request
	// is guaranteed to finish before the thread exists, so it is safe to allocate on the stack. The alternative here
	// would be allocating a buffer for every packet that comes in, which is redundant and un-necessary.
	AsyncIOHeader read_io_hdr;
	bool read_io_active = QueueRead(self, &read_io_hdr);

	// Run until we're deactivated.
	while (self->mHCompletionPort != INVALID_HANDLE_VALUE)
	{
		DWORD bytes_transferred;
		ULONG_PTR completion_key;
		OVERLAPPED* overlapped;
		BOOL result = GetQueuedCompletionStatus(self->mHCompletionPort, &bytes_transferred, &completion_key, &overlapped, INFINITE);

		// GetQueuedCompletionStatus returns false if the IO request encountered an error, or was canceled.
		// If overlapped is set to null, no packet was dequeued. See MSDN docs.
		if (!result && !overlapped)
		{
			WARN_LOG(SP1, "GetQueuedCompletionStatus: err 0x%X", GetLastError());
			continue;
		}

		// For read/write events on the file handle, completion_key is set to self.
		// If the completion key was null, this was a "close" packet queued by CEXIETHERNET::Deactivate.
		if (completion_key == reinterpret_cast<ULONG_PTR>(self))
		{
			// Calculate the address of the containing structure, ASyncIORequest, from the address of the overlapped member.
			AsyncIOHeader* io_hdr = CONTAINING_RECORD(overlapped, AsyncIOHeader, overlapped);
			if (io_hdr->type == AsyncIOHeader::IO_PACKET_TYPE_READ)
			{
				// Pass the size back to the hardware interface, and fire the interrupt.
				_dbg_assert_(SP1, read_io_active);
				if (self->readEnabled.load())
				{
					DEBUG_LOG(SP1, "Received %u bytes\n: %s", bytes_transferred, ArrayToString(self->mRecvBuffer, bytes_transferred, 0x10).c_str());
					self->mRecvBufferLength = bytes_transferred;
					self->RecvHandlePacket();
				}

				// Re-use the same read buffer, since it's been copied into HW memory, and queue another read request.
				read_io_active = QueueRead(self, &read_io_hdr);
			}
			else if (io_hdr->type == AsyncIOHeader::IO_PACKET_TYPE_WRITE)
			{
				// Cast to the write buffer class, to free (if any) buffer allocated with the write.
				// See the note in CEXIETHERNET::SendFrame for why.
				delete static_cast<AsyncIOWritePacket*>(io_hdr);
			}
		}
		else
		{
			// Cancel any outstanding requests from both this thread (reads), and the CPU thread (writes).
			CancelIoEx(self->mHAdapter, nullptr);
			while (true)
			{
				// Keep dequeuing packets from the queue until it is empty.
				// This will include the read request queued above, or any outstanding writes.
				result = GetQueuedCompletionStatus(self->mHCompletionPort, &bytes_transferred, &completion_key, &overlapped, 0);
				if (!result && !overlapped)
					break;

				// Free write packet buffers. See above.
				AsyncIOHeader* io_hdr = CONTAINING_RECORD(overlapped, AsyncIOHeader, overlapped);
				if (io_hdr->type == AsyncIOHeader::IO_PACKET_TYPE_WRITE)
					delete static_cast<AsyncIOWritePacket*>(io_hdr);
			}

			return;
		}
	}
}

bool CEXIETHERNET::SendFrame(u8* frame, u32 size)
{
	DEBUG_LOG(SP1, "SendFrame %u bytes:\n%s",
		size, ArrayToString(frame, size, 0x10).c_str());

	// See comment above the struct declaration for an explanation on the allocation behavior here.
	// Basically, for small packets, we store them in the same memory block as the OVERLAPPED structure,
	// otherwise we allocate a second block of memory. The reason for this ugliness is because we have
	// to keep both the buffer and OVERLAPPED structure around until the kernel has finished with it,
	// and delivers us a completion packet. The memory is then freed afterwards, in ReadThreadHandler.
	AsyncIOWritePacket* packet = new AsyncIOWritePacket;
	u8* frame_buffer = packet->small_frame_buffer;
	if (size > SMALL_FRAME_BUFFER_SIZE)
	{
		packet->frame_buffer = std::make_unique<u8[]>(size);
		frame_buffer = packet->frame_buffer.get();
	}

	// Initialize the packet type, and copy the data into the buffer, whereever that is.
	packet->type = AsyncIOHeader::IO_PACKET_TYPE_WRITE;
	ZeroMemory(&packet->overlapped, sizeof(packet->overlapped));
	memcpy(frame_buffer, frame, size);

	// Invoke asynchronous WriteFile. The request will complete on the IO thread.
	WriteFile(mHAdapter, frame_buffer, size, nullptr, &packet->overlapped);

	// Check something else did not go wrong, and the request was queued.
	DWORD err = GetLastError();
	if (err != ERROR_IO_PENDING)
	{
		ERROR_LOG(SP1, "WriteFile failed (err=0x%X)", err);
		delete packet;
		return false;
	}

	// Always report the packet as being sent successfully, even though it might be a lie
	SendComplete();
	return true;
}

bool CEXIETHERNET::RecvInit()
{
	readEnabled.store(false);
	readThread = std::thread(ReadThreadHandler, this);
	return true;
}

bool CEXIETHERNET::RecvStart()
{
	if (!IsActivated())
		return false;

	readEnabled.store(true);
	return true;
}

void CEXIETHERNET::RecvStop()
{
	if (!IsActivated())
		return;

	readEnabled.store(false);
}
