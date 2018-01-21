// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <list>
#include <string>
#include <vector>

struct LdrDllLoadEvent
{
  const std::wstring& name;
  uintptr_t base_address;
};

class LdrObserver
{
public:
  std::vector<std::wstring> module_names;
  // NOTE: This may be called from a Ldr callout. While most things are probably fine, try to
  // keep things as simple as possible, and just queue real work onto something else.
  std::function<void(const LdrDllLoadEvent&)> action;
  void* cookie{};
  LdrObserver(std::vector<std::wstring> m, std::function<void(const LdrDllLoadEvent&)> a)
      : module_names(m), action(a)
  {
  }
};

class LdrWatcher
{
public:
  void Install(const LdrObserver& observer);
  ~LdrWatcher();

private:
  bool InjectCurrentModules(const LdrObserver& observer);
  void UninstallAll();
  std::list<LdrObserver> observers;
};
