// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
