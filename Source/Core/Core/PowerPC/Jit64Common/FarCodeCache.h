// Copyright 2016 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/x64Emitter.h"

// a bit of a hack; the MMU results in a vast amount more code ending up in the far cache,
// mostly exception handling, so give it a whole bunch more space if the MMU is on.
constexpr int FARCODE_SIZE = 1024 * 1024 * 8;
constexpr int FARCODE_SIZE_MMU = 1024 * 1024 * 48;

// A place to throw blocks of code we don't want polluting the cache, e.g. rarely taken
// exception branches.
class FarCodeCache : public Gen::X64CodeBlock
{
public:
  void Init(int size);
  void Shutdown();

  bool Enabled() const;

private:
  bool m_enabled = false;
};
