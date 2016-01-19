// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#ifdef __unix__

#include <fstream>
#include <sstream>
#include <unistd.h>

#elif defined(_WIN32)

#include <windows.h>

#endif

#include <iostream>

#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/Thread.h"
#include "Core/MemoryWatcher.h"
#include "Core/HW/Memmap.h"

// We don't want to kill the cpu, so sleep for this long after polling.
static const int SLEEP_DURATION = 2; // ms

MemoryWatcher::MemoryWatcher()
{
	if (!LoadAddresses(File::GetUserPath(F_MEMORYWATCHERLOCATIONS_IDX)))
		return;
	if (!OpenSocket(File::GetUserPath(F_MEMORYWATCHERSOCKET_IDX)))
		return;
	m_running = true;
	m_watcher_thread = std::thread(&MemoryWatcher::WatcherThread, this);
}

MemoryWatcher::~MemoryWatcher()
{
	if (!m_running)
		return;

	m_running = false;
	m_watcher_thread.join();

#ifdef __unix__
	close(m_fd);
#elif defined(_WIN32)
	CloseHandle(m_pipe);
#endif
}

bool MemoryWatcher::LoadAddresses(const std::string& path)
{
	std::ifstream locations(path);
	if (!locations)
		return false;

	std::string line;
	while (std::getline(locations, line))
		ParseLine(line);

	return m_values.size() > 0;
}

void MemoryWatcher::ParseLine(const std::string& line)
{
	m_values[line] = 0;
	m_addresses[line] = std::vector<u32>();

	std::stringstream offsets(line);
	offsets >> std::hex;
	u32 offset;
	while (offsets >> offset)
		m_addresses[line].push_back(offset);
}

bool MemoryWatcher::OpenSocket(const std::string& path)
{
#ifdef __unix__
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sun_family = AF_UNIX;
	strncpy(m_addr.sun_path, path.c_str(), sizeof(m_addr.sun_path) - 1);

	m_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	return m_fd >= 0;
#elif defined(_WIN32)
	m_pipe = CreateNamedPipe(L"\\\\.\\dolphin-emu\\mem-watch", 
		PIPE_ACCESS_OUTBOUND,
		PIPE_TYPE_BYTE | PIPE_WAIT,
		1,
		0,
		0,
		0,
		nullptr);

	if(m_pipe == INVALID_HANDLE_VALUE) return false;
	return true;
#endif
}

u32 MemoryWatcher::ChasePointer(const std::string& line)
{
	u32 value = 0;
	for (u32 offset : m_addresses[line])
		value = Memory::Read_U32(value + offset);
	return value;
}

std::string MemoryWatcher::ComposeMessage(const std::string& line, u32 value)
{
	std::stringstream message_stream;
	message_stream << line << '\n' << std::hex << value;
	return message_stream.str();
}

void MemoryWatcher::WatcherThread()
{
#ifdef _WIN32
	if(!ConnectNamedPipe(m_pipe, NULL))
	{
		INFO_LOG("Unable to connect WIN32 named pipe for MemoryWatcher.");
		return;
	}
#endif

	while (m_running)
	{
		for (auto& entry : m_values)
		{
			std::string address = entry.first;
			u32& current_value = entry.second;

			u32 new_value = ChasePointer(address);
			if (new_value != current_value)
			{
				// Update the value
				current_value = new_value;
				std::string message = ComposeMessage(address, new_value);

#ifdef __unix__
				sendto(
					m_fd,
					message.c_str(),
					message.size() + 1,
					0,
					reinterpret_cast<sockaddr*>(&m_addr),
					sizeof(m_addr));
#elif defined(_WIN32)
				if(!WriteFile(m_pipe, message, sizeof(char) * strlen(message), NULL, NULL))
				{
					INFO_LOG("Unable to update memory addresses for MemoryWatcher.");
				}
#endif
			}
		}
		Common::SleepCurrentThread(SLEEP_DURATION);
	}
}
