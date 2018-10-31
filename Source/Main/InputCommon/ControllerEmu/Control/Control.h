// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

class ControlReference;

namespace ControllerEmu
{
enum Translatability
{
  DoNotTranslate,
  Translate
};

class Control
{
public:
  virtual ~Control();

  std::unique_ptr<ControlReference> const control_ref;
  const Translatability translate;
  const std::string name;
  const std::string ui_name;

protected:
  Control(std::unique_ptr<ControlReference> ref, Translatability translate, const std::string& name,
          const std::string& ui_name);
  Control(std::unique_ptr<ControlReference> ref, Translatability translate,
          const std::string& name);
};
}  // namespace ControllerEmu
