// Copyright 2018 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <variant>

namespace Common
{
template <typename Expected, typename Unexpected>
class Result final
{
public:
  constexpr Result(const Expected& value) : m_variant{value} {}
  constexpr Result(Expected&& value) : m_variant{std::move(value)} {}

  constexpr Result(const Unexpected& value) : m_variant{value} {}
  constexpr Result(Unexpected&& value) : m_variant{std::move(value)} {}

  constexpr explicit operator bool() const { return Succeeded(); }
  constexpr bool Succeeded() const { return std::holds_alternative<Expected>(m_variant); }

  // Must only be called when Succeeded() returns true.
  constexpr const Expected& operator*() const { return std::get<Expected>(m_variant); }
  constexpr const Expected* operator->() const { return &std::get<Expected>(m_variant); }
  constexpr Expected& operator*() { return std::get<Expected>(m_variant); }
  constexpr Expected* operator->() { return &std::get<Expected>(m_variant); }

  // Must only be called when Succeeded() returns false.
  constexpr Unexpected& Error() { return std::get<Unexpected>(m_variant); }
  constexpr const Unexpected& Error() const { return std::get<Unexpected>(m_variant); }

private:
  std::variant<Expected, Unexpected> m_variant;
};
}  // namespace Common
