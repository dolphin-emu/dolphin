// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#ifdef _WIN32
#include <comdef.h>
#endif

struct cubeb;

namespace CubebUtils
{
// Helper to initialize and uninitialize thread access for the libraries that cubeb uses.
// It's not necessary to return in case of failure as following functions will also fail
class ScopedThreadAccess
{
public:
  ScopedThreadAccess();
  ~ScopedThreadAccess();

  bool Succeeded() const;

private:
#ifdef _WIN32
  HRESULT result;
#endif
};

std::shared_ptr<cubeb> GetContext();
}  // namespace CubebUtils
