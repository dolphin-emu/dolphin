// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Common::GekkoAssembler::detail
{
// Hacky implementation of a case insensitive alphanumeric trie supporting extended entries
// Standing in for std::map to support case-insensitive lookups while allowing string_views in
// lookups
template <typename V, char... ExtraMatches>
class CaseInsensitiveDict
{
public:
  CaseInsensitiveDict(const std::initializer_list<std::pair<std::string_view, V>>& il)
  {
    for (auto&& [k, v] : il)
    {
      Add(k, v);
    }
  }

  template <typename T>
  V const* Find(const T& key) const
  {
    auto&& [last_e, it] = TryFind(key);
    if (it == key.cend() && last_e->_val)
    {
      return &*last_e->_val;
    }
    return nullptr;
  }
  static constexpr size_t NUM_CONNS = 36 + sizeof...(ExtraMatches);
  static constexpr uint32_t INVALID_CONN = static_cast<uint32_t>(-1);

private:
  struct TrieEntry
  {
    std::array<uint32_t, 36 + sizeof...(ExtraMatches)> _conns;
    std::optional<V> _val;

    TrieEntry() { std::fill(_conns.begin(), _conns.end(), INVALID_CONN); }
  };

  constexpr size_t IndexOf(char c) const
  {
    size_t idx;
    if (std::isalpha(c))
    {
      idx = std::tolower(c) - 'a';
    }
    else if (std::isdigit(c))
    {
      idx = c - '0' + 26;
    }
    else
    {
      idx = 36;
      // Expands to an equivalent for loop over ExtraMatches
      if constexpr (sizeof...(ExtraMatches) > 0)
      {
        (void)((c != ExtraMatches ? ++idx, true : false) && ...);
      }
    }
    return idx;
  }

  template <typename T>
  auto TryFind(const T& key) const -> std::pair<TrieEntry const*, decltype(key.cbegin())>
  {
    std::pair<TrieEntry const*, decltype(key.cbegin())> ret(&m_root_entry, key.cbegin());
    const auto k_end = key.cend();

    for (; ret.second != k_end; ret.second++)
    {
      const size_t idx = IndexOf(*ret.second);
      if (idx >= NUM_CONNS || ret.first->_conns[idx] == INVALID_CONN)
      {
        break;
      }

      ret.first = &m_entry_pool[ret.first->_conns[idx]];
    }

    return ret;
  }

  template <typename T>
  auto TryFind(const T& key) -> std::pair<TrieEntry*, decltype(key.cbegin())>
  {
    auto&& [e_const, it] =
        const_cast<CaseInsensitiveDict<V, ExtraMatches...> const*>(this)->TryFind(key);
    return {const_cast<TrieEntry*>(e_const), it};
  }

  void Add(std::string_view key, const V& val)
  {
    auto&& [last_e, it] = TryFind(key);
    if (it != key.cend())
    {
      for (; it != key.cend(); it++)
      {
        const size_t idx = IndexOf(*it);
        if (idx >= NUM_CONNS)
        {
          break;
        }
        last_e->_conns[idx] = static_cast<uint32_t>(m_entry_pool.size());
        last_e = &m_entry_pool.emplace_back();
      }
    }
    last_e->_val = val;
  }

  TrieEntry m_root_entry;
  std::vector<TrieEntry> m_entry_pool;
};
}  // namespace Common::GekkoAssembler::detail
