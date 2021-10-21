// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/System.h"

namespace Core
{
struct System::Impl
{
};

System::System() : m_impl{std::make_unique<Impl>()}
{
}

System::~System() = default;
}  // namespace Core
