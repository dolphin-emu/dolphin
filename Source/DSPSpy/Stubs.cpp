// Copyright 2003 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// Stubs to make DSPCore compile as part of DSPSpy.

/*
#include <stdio.h>
#include <stdlib.h>

#include <string>

#include "Thread.h"

void *AllocateMemoryPages(size_t size)
{
  return malloc(size);
}

void FreeMemoryPages(void *pages, size_t size)
{
  free(pages);
}

void WriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
}

void UnWriteProtectMemory(void* ptr, size_t size, bool allowExecute)
{
}

bool DSPHost_OnThread()
{
  return false;
}

// Well, it's just RAM right? :)
u8 DSPHost_ReadHostMemory(u32 address)
{
  u8 *ptr = (u8*)address;
  return *ptr;
}

void DSPHost_WriteHostMemory(u8 value, u32 addr) {}


void DSPHost_CodeLoaded(const u8 *code, int size)
{
}


namespace Common
{

CriticalSection::CriticalSection(int)
{
}
CriticalSection::~CriticalSection()
{
}

void CriticalSection::Enter()
{
}

void CriticalSection::Leave()
{
}

}  // namespace

namespace File
{

bool WriteStringToFile(bool text_file, const std::string &str, const char *filename)
{
  FILE *f = fopen(filename, text_file ? "w" : "wb");
  if (!f)
    return false;
  size_t len = str.size();
  if (len != fwrite(str.data(), 1, str.size(), f))
  {
    fclose(f);
    return false;
  }
  fclose(f);
  return true;
}

bool ReadFileToString(bool text_file, const char *filename, std::string &str)
{
  FILE *f = fopen(filename, text_file ? "r" : "rb");
  if (!f)
    return false;
  fseeko(f, 0, SEEK_END);
  size_t len = ftello(f);
  fseeko(f, 0, SEEK_SET);
  char *buf = new char[len + 1];
  buf[fread(buf, 1, len, f)] = 0;
  str = std::string(buf, len);
  fclose(f);
  delete [] buf;
  return true;
}

}

*/
