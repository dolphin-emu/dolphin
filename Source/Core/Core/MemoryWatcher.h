// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>
#include <sys/socket.h>
#include <sys/un.h>
#include <vector>

// MemoryWatcher reads a file containing in-game memory addresses and outputs
// changes to those memory addresses to a unix domain socket as the game runs.
//
// The input file is a newline-separated list of hex memory addresses, without
// the "0x". This can take one of three forms:
// 1: Read four bytes at static memory address
//    ex:
//        ABCD
//    Output: Two lines. The first is the address from the
//      input file, and the second is the new value in hex.
// 2: Read four bytes, followed at pointer
//    ex:
//        ABCD EF
//        ^ will watch the address at (*0xABCD) + 0xEF.
//    Output: Two lines. The first is the address from the
//      input file, and the second is the new value in hex.
// 3: Read linked list structure, with data in pointers
//    If you're wondering what would use such a funny and specific data
//      structure: Items in Melee, that's what.
//    ex:
//        ABCD EF GH
//        ^ ABCD: Address of the initial pointer
//          EF: Size (in bytes) of an element of the linked list
//          GH: Offset within element to pointer to next element in list
//          IJ: Data struct pointer offset
//          KL: Data struct size
//    NOTE: MemoryWatcher will follow the linked list until the pointer to the
//      next element is NULL.
//    Output: N lines, where N is the number of objects found
//      Each line contains space separated hex strings in the following format:
//        ABCD DEADBEEF
//        ^ ABCD: The initial address as passed in initially
//          DEADBEEF: The hex contents of the object in two width string format.

typedef std::vector<unsigned char> Blob;
typedef std::vector<Blob> ListData;
struct linked_list
{
  u32 m_pointer_offset;
  u32 m_data_pointer_offset;
  u32 m_data_struct_len;
  ListData m_data;
};

class MemoryWatcher final
{
public:
  MemoryWatcher();
  ~MemoryWatcher();
  void Step();

  static void Init();
  static void Shutdown();

private:
  bool LoadAddresses(const std::string& path);
  bool OpenSocket(const std::string& path);

  void ParseLine(const std::string& line);
  u32 ChasePointer(const std::string& line);
  // Returns true if the contents changed
  bool ChaseLinkedList(const std::string& address, linked_list &llist);
  std::string ComposeMessage(const std::string& line, u32 value);
  std::string ComposeMessage(const std::string& line, Blob);

  bool IsIdentical(Blob, Blob);

  bool m_running;

  int m_fd;
  sockaddr_un m_addr;

  // Address as stored in the file -> list of offsets to follow
  std::map<std::string, std::vector<u32>> m_addresses;
  // Address as stored in the file -> current value
  std::map<std::string, u32> m_values;

  // Starting address as stored in the file -> vector of data blobs
  std::map<std::string, linked_list> m_linked_lists;
};
