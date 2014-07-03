// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

// windows crap
#define NOMINMAX

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "InputCommon/GCPadStatus.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"

#define sign(x) ((x)?(x)<0?-1:1:0)

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
	GROUP_TYPE_UDPWII,
	GROUP_TYPE_SLIDER,
};

enum
{
	SETTING_RADIUS,
	SETTING_DEADZONE,
	SETTING_SQUARE,
};

const char* const named_directions[] =
{
	"Up",
	"Down",
	"Left",
	"Right"
};

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
				: control_ref(_ref), name(_name) {}

		public:
			virtual ~Control() {}
			std::unique_ptr<ControllerInterface::ControlReference> const control_ref;
			const std::string name;

		};

		class Input : public Control
		{
		public:

			Input(const std::string& _name)
				: Control(new ControllerInterface::InputReference, _name) {}
		};

		class Output : public Control
		{
		public:

			Output(const std::string& _name)
				: Control(new ControllerInterface::OutputReference, _name) {}
		};

		class Setting
		{
		public:

			Setting(const std::string& _name, const ControlState def_value
				, const unsigned int _low = 0, const unsigned int _high = 100)
				: name(_name)
				, value(def_value)
				, default_value(def_value)
				, low(_low)
				, high(_high){}

			const std::string   name;
			ControlState        value;
			const ControlState  default_value;
			const unsigned int  low, high;
		};

		ControlGroup(const std::string& _name, const unsigned int _type = GROUP_TYPE_OTHER) : name(_name), type(_type) {}
		virtual ~ControlGroup() {}

		virtual void LoadConfig(IniFile::Section *sec, const std::string& defdev = "", const std::string& base = "" );
		virtual void SaveConfig(IniFile::Section *sec, const std::string& defdev = "", const std::string& base = "" );

		const std::string     name;
		const unsigned int    type;

		std::vector<std::unique_ptr<Control>> controls;
		std::vector<std::unique_ptr<Setting>> settings;

	};

	class AnalogStick : public ControlGroup
	{
	public:

		template <typename C>
		void GetState(C* const x, C* const y, const unsigned int base, const unsigned int range)
		{
			// this is all a mess

			ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
			ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

			ControlState radius = settings[SETTING_RADIUS]->value;
			ControlState deadzone = settings[SETTING_DEADZONE]->value;
			ControlState square = settings[SETTING_SQUARE]->value;
			ControlState m = controls[4]->control_ref->State();

			// modifier code
			if (m)
			{
				yy = (fabsf(yy)>deadzone) * sign(yy) * (m + deadzone/2);
				xx = (fabsf(xx)>deadzone) * sign(xx) * (m + deadzone/2);
			}

			// deadzone / square stick code
			if (radius != 1 || deadzone || square)
			{
				// this section might be all wrong, but its working good enough, I think

				ControlState ang = atan2(yy, xx);
				ControlState ang_sin = sin(ang);
				ControlState ang_cos = cos(ang);

				// the amt a full square stick would have at current angle
				ControlState square_full = std::min(ang_sin ? 1/fabsf(ang_sin) : 2, ang_cos ? 1/fabsf(ang_cos) : 2);

				// the amt a full stick would have that was ( user setting squareness) at current angle
				// I think this is more like a pointed circle rather than a rounded square like it should be
				ControlState stick_full = (1 + (square_full - 1) * square);

				ControlState dist = sqrt(xx*xx + yy*yy);

				// dead zone code
				dist = std::max(0.0f, dist - deadzone * stick_full);
				dist /= (1 - deadzone);

				// square stick code
				ControlState amt = dist / stick_full;
				dist -= ((square_full - 1) * amt * square);

				// radius
				dist *= radius;

				yy = std::max(-1.0f, std::min(1.0f, ang_sin * dist));
				xx = std::max(-1.0f, std::min(1.0f, ang_cos * dist));
			}

			*y = C(yy * range + base);
			*x = C(xx * range + base);
		}

		AnalogStick(const char* const _name);

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
				if (control->control_ref->State() > settings[0]->value) // threshold
					*buttons |= *bitmasks;

				bitmasks++;
			}
		}

	};

	class MixedTriggers : public ControlGroup
	{
	public:

		template <typename C, typename S>
		void GetState(C* const digital, const C* bitmasks, S* analog, const unsigned int range)
		{
			const unsigned int trig_count = ((unsigned int) (controls.size() / 2));
			for (unsigned int i=0; i<trig_count; ++i,++bitmasks,++analog)
			{
				if (controls[i]->control_ref->State() > settings[0]->value) //threshold
				{
					*analog = range;
					*digital |= *bitmasks;
				}
				else
				{
					*analog = S(controls[i+trig_count]->control_ref->State() * range);
				}
			}
		}

		MixedTriggers(const std::string& _name);

	};

	class Triggers : public ControlGroup
	{
	public:

		template <typename S>
		void GetState(S* analog, const unsigned int range)
		{
			const unsigned int trig_count = ((unsigned int) (controls.size()));
			const ControlState deadzone = settings[0]->value;
			for (unsigned int i=0; i<trig_count; ++i,++analog)
				*analog = S(std::max(controls[i]->control_ref->State() - deadzone, 0.0f) / (1 - deadzone) * range);
		}

		Triggers(const std::string& _name);

	};

	class Slider : public ControlGroup
	{
	public:

		template <typename S>
		void GetState(S* const slider, const unsigned int range, const unsigned int base = 0)
		{
			const float deadzone = settings[0]->value;
			const float state = controls[1]->control_ref->State() - controls[0]->control_ref->State();

			if (fabsf(state) > deadzone)
				*slider = (S)((state - (deadzone * sign(state))) / (1 - deadzone) * range + base);
			else
				*slider = 0;
		}

		Slider(const std::string& _name);

	};

	class Force : public ControlGroup
	{
	public:
		Force(const std::string& _name);

		template <typename C, typename R>
		void GetState(C* axis, const u8 base, const R range)
		{
			const float deadzone = settings[0]->value;
			for (unsigned int i=0; i<6; i+=2)
			{
				float tmpf = 0;
				const float state = controls[i+1]->control_ref->State() - controls[i]->control_ref->State();
				if (fabsf(state) > deadzone)
					tmpf = ((state - (deadzone * sign(state))) / (1 - deadzone));

				float &ax = m_swing[i >> 1];
				*axis++ = (C)((tmpf - ax) * range + base);
				ax = tmpf;
			}
		}

	private:
		float m_swing[3];
	};

	class Tilt : public ControlGroup
	{
	public:
		Tilt(const std::string& _name);

		template <typename C, typename R>
		void GetState(C* const x, C* const y, const unsigned int base, const R range, const bool step = true)
		{
			// this is all a mess

			ControlState yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
			ControlState xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

			ControlState deadzone = settings[0]->value;
			ControlState circle = settings[1]->value;
			auto const angle = settings[2]->value / 1.8f;
			ControlState m = controls[4]->control_ref->State();

			// modifier code
			if (m)
			{
				yy = (fabsf(yy)>deadzone) * sign(yy) * (m + deadzone/2);
				xx = (fabsf(xx)>deadzone) * sign(xx) * (m + deadzone/2);
			}

			// deadzone / circle stick code
			if (deadzone || circle)
			{
				// this section might be all wrong, but its working good enough, I think

				ControlState ang = atan2(yy, xx);
				ControlState ang_sin = sin(ang);
				ControlState ang_cos = cos(ang);

				// the amt a full square stick would have at current angle
				ControlState square_full = std::min(ang_sin ? 1/fabsf(ang_sin) : 2, ang_cos ? 1/fabsf(ang_cos) : 2);

				// the amt a full stick would have that was (user setting circular) at current angle
				// I think this is more like a pointed circle rather than a rounded square like it should be
				ControlState stick_full = (square_full * (1 - circle)) + (circle);

				ControlState dist = sqrt(xx*xx + yy*yy);

				// dead zone code
				dist = std::max(0.0f, dist - deadzone * stick_full);
				dist /= (1 - deadzone);

				// circle stick code
				ControlState amt = dist / stick_full;
				dist += (square_full - 1) * amt * circle;

				yy = std::max(-1.0f, std::min(1.0f, ang_sin * dist));
				xx = std::max(-1.0f, std::min(1.0f, ang_cos * dist));
			}

			// this is kinda silly here
			// gui being open will make this happen 2x as fast, o well

			// silly
			if (step)
			{
				if (xx > m_tilt[0])
					m_tilt[0] = std::min(m_tilt[0] + 0.1f, xx);
				else if (xx < m_tilt[0])
					m_tilt[0] = std::max(m_tilt[0] - 0.1f, xx);

				if (yy > m_tilt[1])
					m_tilt[1] = std::min(m_tilt[1] + 0.1f, yy);
				else if (yy < m_tilt[1])
					m_tilt[1] = std::max(m_tilt[1] - 0.1f, yy);
			}

			*y = C(m_tilt[1] * angle * range + base);
			*x = C(m_tilt[0] * angle * range + base);
		}

	private:
		float m_tilt[2];
	};

	class Cursor : public ControlGroup
	{
	public:
		Cursor(const std::string& _name);

		template <typename C>
		void GetState(C* const x, C* const y, C* const z, const bool adjusted = false)
		{
			const float zz = controls[4]->control_ref->State() - controls[5]->control_ref->State();

			// silly being here
			if (zz > m_z)
				m_z = std::min(m_z + 0.1f, zz);
			else if (zz < m_z)
				m_z = std::max(m_z - 0.1f, zz);

			*z = m_z;

			// hide
			if (controls[6]->control_ref->State() > 0.5f)
			{
				*x = 10000; *y = 0;
			}
			else
			{
				float yy = controls[0]->control_ref->State() - controls[1]->control_ref->State();
				float xx = controls[3]->control_ref->State() - controls[2]->control_ref->State();

				// adjust cursor according to settings
				if (adjusted)
				{
					xx *= (settings[1]->value * 2);
					yy *= (settings[2]->value * 2);
					yy += (settings[0]->value - 0.5f);
				}

				*x = xx;
				*y = yy;
			}
		}

		float m_z;
	};

	class Extension : public ControlGroup
	{
	public:
		Extension(const std::string& _name)
			: ControlGroup(_name, GROUP_TYPE_EXTENSION)
			, switch_extension(0)
			, active_extension(0) {}

		~Extension() {}

		void GetState(u8* const data, const bool focus = true);

		std::vector<std::unique_ptr<ControllerEmu>> attachments;

		int switch_extension;
		int active_extension;
	};

	virtual ~ControllerEmu() {}

	virtual std::string GetName() const = 0;

	virtual void LoadDefaults(const ControllerInterface& ciface);

	virtual void LoadConfig(IniFile::Section *sec, const std::string& base = "");
	virtual void SaveConfig(IniFile::Section *sec, const std::string& base = "");
	void UpdateDefaultDevice();

	void UpdateReferences(ControllerInterface& devi);

	std::vector<std::unique_ptr<ControlGroup>> groups;

	DeviceQualifier default_device;
};
