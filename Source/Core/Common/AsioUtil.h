// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <exception>

#include <asio/detail/throw_exception.hpp>

#include "Common/Logging/Log.h"

namespace asio::detail
{
// This function must be defined when using `ASIO_NO_EXCEPTIONS`.
// It should never be invoked. Use `asio::error_code` overloads that don't throw exceptions.
template <typename Exception>
[[noreturn]] void throw_exception(const Exception& e)
{
  ERROR_LOG_FMT(COMMON, "asio exception: {}", e.what());
  std::terminate();
}
}  // namespace asio::detail
