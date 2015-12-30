// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <fstream>
#include <iostream>

#include "Common/FileUtil.h"
#include "Common/Thread.h"
#include "Core/MemoryWatcher.h"
#include "Core/HW/Memmap.h"

// We don't want to kill the cpu, so sleep for this long after polling.
static const int SLEEP_DURATION = 10; // ms

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
	close(m_fd);
}

bool MemoryWatcher::LoadAddresses(const std::string& path)
{
	std::ifstream locations(path);
	if (!locations)
		return false;

	u32 data;
	locations >> std::hex;
	while (locations >> data)
		m_values[data] = 0;

	return m_values.size() > 0;
}

bool MemoryWatcher::OpenSocket(const std::string& path)
{
	memset(&m_addr, 0, sizeof(m_addr));
	m_addr.sun_family = AF_UNIX;
	strncpy(m_addr.sun_path, path.c_str(), sizeof(m_addr.sun_path) - 1);

	m_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	return m_fd >= 0;
}

void MemoryWatcher::WatcherThread()
{
	while (m_running)
	{
		for (auto& entry : m_values)
		{
			u32 address = entry.first;
			u32& current_value = entry.second;

			u32 new_value = Memory::Read_U32(address);
			if (new_value != current_value)
			{
				current_value = new_value;
				u32 buf[2] = {address, current_value};
				sendto(
					m_fd,
					static_cast<void*>(buf),
					sizeof(buf),
					0,
					reinterpret_cast<sockaddr*>(&m_addr),
					sizeof(m_addr));
			}
		}
		Common::SleepCurrentThread(SLEEP_DURATION);
	}
}
