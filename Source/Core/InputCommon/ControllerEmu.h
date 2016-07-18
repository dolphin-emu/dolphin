// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "InputCommon/GCPadStatus.h"

#define sign(x) ((x) ? (x) < 0 ? -1 : 1 : 0)

enum
{
  GROUP_TYPE_OTHER,
  GROUP_TYPE_STICK,
  GROUP_TYPE_MIXED_TRIGGERS,
  GROUP_TYPE_BUTTONS,
  GROUP_TYPE_FORCE,
  GROUP_TYPE_EXTENSION,
  GROUP_TYPE_TILT,
  GROUP_TYPE_CURSOR,
  GROUP_TYPE_TRIGGERS,
  GROUP_TYPE_SLIDER
};

enum
{
  SETTING_RADIUS,
  SETTING_DEADZONE,
};

const char* const named_directions[] = {"Up", "Down", "Left", "Right"};

class ControllerEmu
{
public:
  class ControlGroup
  {
  public:
    class Control
    {
    protected:
      Control(ControllerInterface::ControlReference* const _ref, const std::string& _name)
          : control_ref(_ref), name(_name)
      {
      }

    public:
      virtual ~Control() {}
      std::unique_ptr<ControllerInterface::ControlReference> const control_ref;
      const std::string name;
    };

    class Input : public Control
    {
    public:
      Input(const std::string& _name) : Control(new ControllerInterface::InputReference, _name) {}
    };

    class Output : public Control
    {
    public:
      Output(const std::string& _name) : Control(new ControllerInterface::OutputReference, _name) {}
    };

    enum class SettingType
    {
      NORMAL,   // normal settings are saved to configuration files
      VIRTUAL,  // virtual settings are not saved at all
    };

    class NumericSetting
    {
    public:
      NumericSetting(const std::string& setting_name, const ControlState default_value,
                     const unsigned int low = 0, const unsigned int high = 100,
                     const SettingType setting_type = SettingType::NORMAL)
          : m_type(setting_type), m_name(setting_name), m_default_value(default_value), m_low(low),
            m_high(high)
      {
      }

      ControlState GetValue() const { return m_value; }
      void SetValue(ControlState value) { m_value = value; }
      const SettingType m_type;
      const std::string m_name;
      const ControlState m_default_value;
      const unsigned int m_low, m_high;
      ControlState m_value;
    };

    class BooleanSetting
    {
    public:
      BooleanSetting(const std::string& setting_name, const bool default_value,
                     const SettingType setting_type = SettingType::NORMAL)
          : m_type(setting_type), m_name(setting_name), m_default_value(default_value)
      {
      }

      virtual bool GetValue() const { return m_value; }
      virtual void SetValue(bool value) { m_value = value; }
      const SettingType m_type;
      const std::string m_name;
      const bool m_default_value;
      bool m_value;
    };

    class BackgroundInputSetting : public BooleanSetting
    {
    public:
      BackgroundInputSetting(const std::string& setting_name)
          : BooleanSetting(setting_name, false, SettingType::VIRTUAL)
      {
      }

      bool GetValue() const override { return SConfig::GetInstance().m_BackgroundInput; }
      void SetValue(bool value) override
      {
        m_value = value;
        SConfig::GetInstance().m_BackgroundInput = value;
      }
    };

    ControlGroup(const std::string& _name, const unsigned int _type = GROUP_TYPE_OTHER)
        : name(_name), ui_name(_name), type(_type)
    {
    }
    ControlGroup(const std::string& _name, const std::string& _ui_name,
                 const unsigned int _type = GROUP_TYPE_OTHER)
        : name(_name), ui_name(_ui_name), type(_type)
    {
    }
    virtual ~ControlGroup() {}
    virtual void LoadConfig(IniFile::Section* sec, const std::string& defdev = "",
                            const std::string& base = "");
    virtual void SaveConfig(IniFile::Section* sec, const std::string& defdev = "",
                            const std::string& base = "");

    void SetControlExpression(int index, const std::string& expression);

    const std::string name;
    const std::string ui_name;
    const unsigned int type;

    std::vector<std::unique_ptr<Control>> controls;
    std::vector<std::unique_ptr<NumericSetting>> numeric_settings;
    std::vector<std::unique_ptr<BooleanSetting>> boolean_settings;
  };

  class AnalogStick : public ControlGroup
  {
  public:
    // The GameCube controller and Wiimote attachments have a different default radius
    AnalogStick(const char* const _name, ControlState default_radius);
    AnalogStick(const char* const _name, const char* const _ui_name, ControlState default_radius);

    void GetState(ControlState* const x, ControlState* const y)
    {
      ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
      ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

      ControlState radius = numeric_settings[SETTING_RADIUS]->GetValue();
      ControlState deadzone = numeric_settings[SETTING_DEADZONE]->GetValue();
      ControlState m = controls[4]->control_ref->State();

      ControlState ang = atan2(yy, xx);
      ControlState ang_sin = sin(ang);
      ControlState ang_cos = cos(ang);

      ControlState dist = sqrt(xx * xx + yy * yy);

      // dead zone code
      dist = std::max(0.0, dist - deadzone);
      dist /= (1 - deadzone);

      // radius
      dist *= radius;

      // The modifier halves the distance by 50%, which is useful
      // for keyboard controls.
      if (m)
        dist *= 0.5;

      yy = std::max(-1.0, std::min(1.0, ang_sin * dist));
      xx = std::max(-1.0, std::min(1.0, ang_cos * dist));

      *y = yy;
      *x = xx;
    }
  };

  class Buttons : public ControlGroup
  {
  public:
    Buttons(const std::string& _name);

    template <typename C>
    void GetState(C* const buttons, const C* bitmasks)
    {
      for (auto& control : controls)
      {
        if (control->control_ref->State() > numeric_settings[0]->GetValue())  // threshold
          *buttons |= *bitmasks;

        bitmasks++;
      }
    }
  };

  class MixedTriggers : public ControlGroup
  {
  public:
    MixedTriggers(const std::string& _name);

    void GetState(u16* const digital, const u16* bitmasks, ControlState* analog)
    {
      const unsigned int trig_count = ((unsigned int)(controls.size() / 2));
      for (unsigned int i = 0; i < trig_count; ++i, ++bitmasks, ++analog)
      {
        if (controls[i]->control_ref->State() > numeric_settings[0]->GetValue())  // threshold
        {
          *analog = 1.0;
          *digital |= *bitmasks;
        }
        else
        {
          *analog = controls[i + trig_count]->control_ref->State();
        }
      }
    }
  };

  class Triggers : public ControlGroup
  {
  public:
    Triggers(const std::string& _name);

    void GetState(ControlState* analog)
    {
      const unsigned int trig_count = ((unsigned int)(controls.size()));
      const ControlState deadzone = numeric_settings[0]->GetValue();
      for (unsigned int i = 0; i < trig_count; ++i, ++analog)
        *analog = std::max(controls[i]->control_ref->State() - deadzone, 0.0) / (1 - deadzone);
    }
  };

  class Slider : public ControlGroup
  {
  public:
    Slider(const std::string& _name);

    void GetState(ControlState* const slider)
    {
      const ControlState deadzone = numeric_settings[0]->GetValue();
      const ControlState state =
          controls[1]->control_ref->State() - controls[0]->control_ref->State();

      if (fabs(state) > deadzone)
        *slider = (state - (deadzone * sign(state))) / (1 - deadzone);
      else
        *slider = 0;
    }
  };

  class Force : public ControlGroup
  {
  public:
    Force(const std::string& _name);

    void GetState(ControlState* axis)
    {
      const ControlState deadzone = numeric_settings[0]->GetValue();
      for (unsigned int i = 0; i < 6; i += 2)
      {
        ControlState tmpf = 0;
        const ControlState state =
            controls[i + 1]->control_ref->State() - controls[i]->control_ref->State();
        if (fabs(state) > deadzone)
          tmpf = ((state - (deadzone * sign(state))) / (1 - deadzone));

        ControlState& ax = m_swing[i >> 1];
        *axis++ = (tmpf - ax);
        ax = tmpf;
      }
    }

  private:
    ControlState m_swing[3];
  };

  class Tilt : public ControlGroup
  {
  public:
    Tilt(const std::string& _name);

    void GetState(ControlState* const x, ControlState* const y, const bool step = true)
    {
      // this is all a mess

      ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
      ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

      ControlState deadzone = numeric_settings[0]->GetValue();
      ControlState circle = numeric_settings[1]->GetValue();
      auto const angle = numeric_settings[2]->GetValue() / 1.8;
      ControlState m = controls[4]->control_ref->State();

      // deadzone / circle stick code
      // this section might be all wrong, but its working good enough, I think

      ControlState ang = atan2(yy, xx);
      ControlState ang_sin = sin(ang);
      ControlState ang_cos = cos(ang);

      // the amt a full square stick would have at current angle
      ControlState square_full =
          std::min(ang_sin ? 1 / fabs(ang_sin) : 2, ang_cos ? 1 / fabs(ang_cos) : 2);

      // the amt a full stick would have that was (user setting circular) at current angle
      // I think this is more like a pointed circle rather than a rounded square like it should be
      ControlState stick_full = (square_full * (1 - circle)) + (circle);

      ControlState dist = sqrt(xx * xx + yy * yy);

      // dead zone code
      dist = std::max(0.0, dist - deadzone * stick_full);
      dist /= (1 - deadzone);

      // circle stick code
      ControlState amt = dist / stick_full;
      dist += (square_full - 1) * amt * circle;

      if (m)
        dist *= 0.5;

      yy = std::max(-1.0, std::min(1.0, ang_sin * dist));
      xx = std::max(-1.0, std::min(1.0, ang_cos * dist));

      // this is kinda silly here
      // gui being open will make this happen 2x as fast, o well

      // silly
      if (step)
      {
        if (xx > m_tilt[0])
          m_tilt[0] = std::min(m_tilt[0] + 0.1, xx);
        else if (xx < m_tilt[0])
          m_tilt[0] = std::max(m_tilt[0] - 0.1, xx);

        if (yy > m_tilt[1])
          m_tilt[1] = std::min(m_tilt[1] + 0.1, yy);
        else if (yy < m_tilt[1])
          m_tilt[1] = std::max(m_tilt[1] - 0.1, yy);
      }

      *y = m_tilt[1] * angle;
      *x = m_tilt[0] * angle;
    }

  private:
    ControlState m_tilt[2];
  };

  class Cursor : public ControlGroup
  {
  public:
    Cursor(const std::string& _name);

    void GetState(ControlState* const x, ControlState* const y, ControlState* const z,
                  const bool adjusted = false)
    {
      const ControlState zz = controls[4]->control_ref->State() - controls[5]->control_ref->State();

      // silly being here
      if (zz > m_z)
        m_z = std::min(m_z + 0.1, zz);
      else if (zz < m_z)
        m_z = std::max(m_z - 0.1, zz);

      *z = m_z;

      // hide
      if (controls[6]->control_ref->State() > 0.5)
      {
        *x = 10000;
        *y = 0;
      }
      else
      {
        ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
        ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

        // adjust cursor according to settings
        if (adjusted)
        {
          xx *= (numeric_settings[1]->GetValue() * 2);
          yy *= (numeric_settings[2]->GetValue() * 2);
          yy += (numeric_settings[0]->GetValue() - 0.5);
        }

        *x = xx;
        *y = yy;
      }
    }

    ControlState m_z;
  };

  class Extension : public ControlGroup
  {
  public:
    Extension(const std::string& _name)
        : ControlGroup(_name, GROUP_TYPE_EXTENSION), switch_extension(0), active_extension(0)
    {
    }

    ~Extension() {}
    void GetState(u8* const data);
    bool IsButtonPressed() const;

    std::vector<std::unique_ptr<ControllerEmu>> attachments;

    int switch_extension;
    int active_extension;
  };

  virtual ~ControllerEmu() {}
  virtual std::string GetName() const = 0;

  virtual void LoadDefaults(const ControllerInterface& ciface);

  virtual void LoadConfig(IniFile::Section* sec, const std::string& base = "");
  virtual void SaveConfig(IniFile::Section* sec, const std::string& base = "");
  void UpdateDefaultDevice();

  void UpdateReferences(ControllerInterface& devi);

  std::vector<std::unique_ptr<ControlGroup>> groups;

  ciface::Core::DeviceQualifier default_device;
};
