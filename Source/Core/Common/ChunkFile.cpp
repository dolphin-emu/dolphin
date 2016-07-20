// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/ChunkFile.h"
#include "Common/StringUtil.h"

PointerWrap PointerWrap::CreateForLoad(const void* start, size_t size)
{
  PointerWrap p;
  p.m_ptr = (u8*)start;
  p.m_remaining = size;
  p.m_is_load = true;
  p.m_is_good = true;
  return p;
}
PointerWrap PointerWrap::CreateForStore(MallocBuffer initial_buffer)
{
  PointerWrap p;
  p.m_buffer = std::move(initial_buffer);
  p.m_ptr = p.m_buffer.ptr;
  p.m_remaining = p.m_buffer.length;
  p.m_is_load = false;
  p.m_is_good = true;
  return p;
}

MallocBuffer PointerWrap::TakeBuffer()
{
  _assert_(!m_is_load);
  return std::move(m_buffer);
}

void PointerWrap::SetError(std::string&& error)
{
  m_error = std::move(error);
  m_is_good = false;
}

void PointerWrap::SetErrorTruncatedLoad(u64 req_size)
{
  SetError(StringFromFormat("Truncated data; needed %llu more bytes",
                            (unsigned long long)(req_size - m_remaining)));
}

void PointerWrap::ReallocBufferForRequestedSize(u64 req_size)
{
  _assert_(!m_is_load);
  u8* base = m_buffer.ptr;
  size_t size = m_buffer.length;
  if ((u64)(SIZE_MAX - size) < req_size || SIZE_MAX - (size + req_size) < SLACK ||
      !(m_buffer.ptr =
            (u8*)realloc(m_buffer.ptr, (m_buffer.length = size + (size_t)req_size + SLACK))))
  {
    PanicAlertT("Out of memory saving state");
    Crash();
  }
  m_ptr = m_buffer.ptr + (m_ptr - base);
  m_remaining += req_size + SLACK;
}

void PointerWrap::DoMarker(const std::string& prevName, u32 arbitraryNumber)
{
  u32 cookie = arbitraryNumber;
  Do(cookie);

  if (IsLoad() && cookie != arbitraryNumber)
  {
    SetError(StringFromFormat("After \"%s\", found %d (0x%X) instead of save marker %d (0x%X).",
                              prevName.c_str(), cookie, cookie, arbitraryNumber, arbitraryNumber));
  }
}

u8* PointerWrap::GetPointerAndAdvance(u64 req_size)
{
  u8* here = m_ptr;
  if (IsLoad())
  {
    if (req_size > m_remaining)
    {
      SetErrorTruncatedLoad(req_size);
      return nullptr;
    }
  }
  else
  {
    if (req_size > m_remaining || req_size > m_remaining + SLACK)
      ReallocBufferForRequestedSize(req_size);
  }
  m_ptr = here + (size_t)req_size;
  return here;
}

StateLoadStore StateLoadStore::CreateForLoad(const void* start, size_t size)
{
  StateLoadStore p;
  p.PointerWrap::operator=(PointerWrap::CreateForLoad(start, size));
  return p;
}
StateLoadStore StateLoadStore::CreateForStore(MallocBuffer initial_buffer)
{
  StateLoadStore p;
  p.PointerWrap::operator=(PointerWrap::CreateForStore(std::move(initial_buffer)));
  return p;
}
