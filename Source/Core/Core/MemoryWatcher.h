// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <map>
#include <thread>
#include <sys/socket.h>
#include <sys/un.h>

// MemoryWatcher reads a file containing in-game memory addresses and outputs
// changes to those memory addresses to a unix domain socket as the game runs.
//
// The input file is a newline-separated list of hex memory addresses
// (without the "0x"). The output to the socket is two words long, the first
// containing the address, and the second containing the data as stored in
// game memory.
class MemoryWatcher final
{
public:
	MemoryWatcher();
	~MemoryWatcher();

private:
	bool LoadAddresses(const std::string& path);
	bool OpenSocket(const std::string& path);
	void WatcherThread();

	std::thread m_watcher_thread;
	std::atomic_bool m_running;

	int m_fd;
	sockaddr_un m_addr;

	// Address -> last value
	std::map<u32, u32> m_values;
};
