// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "ControllerEmu.h"

#if defined(HAVE_X11) && HAVE_X11
#include <X11/Xlib.h>
#endif

ControllerEmu::~ControllerEmu()
{
	// control groups
	for (ControlGroup* cg : groups)
		delete cg;
}

ControllerEmu::ControlGroup::~ControlGroup()
{
	for (Control* c : controls)
		delete c;

	for (Setting* s : settings)
		delete s;
}

ControllerEmu::Extension::~Extension()
{
	for (ControllerEmu* ai : attachments)
		delete ai;
}
ControllerEmu::ControlGroup::Control::~Control()
{
	delete control_ref;
}

void ControllerEmu::UpdateReferences(ControllerInterface& devi)
{
	for (ControlGroup* cg : groups)
	{
		for (ControlGroup::Control* control : cg->controls)
			devi.UpdateReference(control->control_ref, default_device);

		// extension
		if (GROUP_TYPE_EXTENSION == cg->type)
		{
			for (ControllerEmu* ai : ((Extension*)cg)->attachments)
				ai->UpdateReferences(devi);
		}
	}
}

void ControllerEmu::UpdateDefaultDevice()
{
	for (ControlGroup* cg : groups)
	{
		// extension
		if (GROUP_TYPE_EXTENSION == cg->type)
		{
			for (ControllerEmu* ai : ((Extension*)cg)->attachments)
			{
				ai->default_device = default_device;
				ai->UpdateDefaultDevice();
			}
		}
	}
}

void ControllerEmu::ControlGroup::LoadConfig(IniFile::Section *sec, const std::string& defdev, const std::string& base)
{
	std::string group(base + name); group += "/";

	// settings
	for (Setting* s : settings)
	{
		sec->Get((group + s->name).c_str(), &s->value, s->default_value * 100);
		s->value /= 100;
	}

	// controls
	for (Control* c : controls)
	{
		// control expression
		sec->Get((group + c->name).c_str(), &c->control_ref->expression, "");

		// range
		sec->Get((group + c->name + "/Range").c_str(), &c->control_ref->range, 100.0f);
		c->control_ref->range /= 100;

	}

	// extensions
	if (GROUP_TYPE_EXTENSION == type)
	{
		Extension* const ext = ((Extension*)this);

		ext->switch_extension = 0;
		unsigned int n = 0;
		std::string extname;
		sec->Get((base + name).c_str(), &extname, "");

		for (ControllerEmu* ai : ext->attachments)
		{
			ai->default_device.FromString(defdev);
			ai->LoadConfig(sec, base + ai->GetName() + "/");

			if (ai->GetName() == extname)
				ext->switch_extension = n;

			n++;
		}
	}
}

void ControllerEmu::LoadConfig(IniFile::Section *sec, const std::string& base)
{
	std::string defdev = default_device.ToString();
	if (base.empty())
	{
		sec->Get((base + "Device").c_str(), &defdev, "");
		default_device.FromString(defdev);
	}

	for (ControlGroup* cg : groups)
		cg->LoadConfig(sec, defdev, base);
}

void ControllerEmu::ControlGroup::SaveConfig(IniFile::Section *sec, const std::string& defdev, const std::string& base)
{
	std::string group(base + name); group += "/";

	// settings
	for (Setting* s : settings)
		sec->Set((group + s->name).c_str(), s->value*100.0f, s->default_value*100.0f);

	// controls
	for (Control* c : controls)
	{
		// control expression
		sec->Set((group + c->name).c_str(), c->control_ref->expression, "");

		// range
		sec->Set((group + c->name + "/Range").c_str(), c->control_ref->range*100.0f, 100.0f);
	}

	// extensions
	if (GROUP_TYPE_EXTENSION == type)
	{
		Extension* const ext = ((Extension*)this);
		sec->Set((base + name).c_str(), ext->attachments[ext->switch_extension]->GetName(), "None");

		for (ControllerEmu* ai : ext->attachments)
			ai->SaveConfig(sec, base + ai->GetName() + "/");
	}
}

void ControllerEmu::SaveConfig(IniFile::Section *sec, const std::string& base)
{
	const std::string defdev = default_device.ToString();
	if (base.empty())
		sec->Set((/*std::string(" ") +*/ base + "Device").c_str(), defdev, "");

	for (ControlGroup* cg : groups)
		cg->SaveConfig(sec, defdev, base);
}

ControllerEmu::AnalogStick::AnalogStick(const char* const _name) : ControlGroup(_name, GROUP_TYPE_STICK)
{
	for (auto& named_direction : named_directions)
		controls.push_back(new Input(named_direction));

	controls.push_back(new Input(_trans("Modifier")));

	settings.push_back(new Setting(_trans("Radius"), 0.7f, 0, 100));
	settings.push_back(new Setting(_trans("Dead Zone"), 0, 0, 50));
	settings.push_back(new Setting(_trans("Square Stick"), 0));

}

ControllerEmu::Buttons::Buttons(const char* const _name) : ControlGroup(_name, GROUP_TYPE_BUTTONS)
{
	settings.push_back(new Setting(_trans("Threshold"), 0.5f));
}

ControllerEmu::MixedTriggers::MixedTriggers(const char* const _name) : ControlGroup(_name, GROUP_TYPE_MIXED_TRIGGERS)
{
	settings.push_back(new Setting(_trans("Threshold"), 0.9f));
}

ControllerEmu::Triggers::Triggers(const char* const _name) : ControlGroup(_name, GROUP_TYPE_TRIGGERS)
{
	settings.push_back(new Setting(_trans("Dead Zone"), 0, 0, 50));
}

ControllerEmu::Slider::Slider(const char* const _name) : ControlGroup(_name, GROUP_TYPE_SLIDER)
{
	controls.push_back(new Input("Left"));
	controls.push_back(new Input("Right"));

	settings.push_back(new Setting(_trans("Dead Zone"), 0, 0, 50));
}

ControllerEmu::Force::Force(const char* const _name) : ControlGroup(_name, GROUP_TYPE_FORCE)
{
	memset(m_swing, 0, sizeof(m_swing));

	controls.push_back(new Input(_trans("Up")));
	controls.push_back(new Input(_trans("Down")));
	controls.push_back(new Input(_trans("Left")));
	controls.push_back(new Input(_trans("Right")));
	controls.push_back(new Input(_trans("Forward")));
	controls.push_back(new Input(_trans("Backward")));

	settings.push_back(new Setting(_trans("Dead Zone"), 0, 0, 50));
}

ControllerEmu::Tilt::Tilt(const char* const _name)
	: ControlGroup(_name, GROUP_TYPE_TILT)
{
	memset(m_tilt, 0, sizeof(m_tilt));

	controls.push_back(new Input("Forward"));
	controls.push_back(new Input("Backward"));
	controls.push_back(new Input("Left"));
	controls.push_back(new Input("Right"));

	controls.push_back(new Input(_trans("Modifier")));

	settings.push_back(new Setting(_trans("Dead Zone"), 0, 0, 50));
	settings.push_back(new Setting(_trans("Circle Stick"), 0));
	settings.push_back(new Setting(_trans("Angle"), 0.9f, 0, 180));
}

ControllerEmu::Cursor::Cursor(const char* const _name)
	: ControlGroup(_name, GROUP_TYPE_CURSOR)
	, m_z(0)
{
	for (auto& named_direction : named_directions)
		controls.push_back(new Input(named_direction));
	controls.push_back(new Input("Forward"));
	controls.push_back(new Input("Backward"));
	controls.push_back(new Input(_trans("Hide")));

	settings.push_back(new Setting(_trans("Center"), 0.5f));
	settings.push_back(new Setting(_trans("Width"), 0.5f));
	settings.push_back(new Setting(_trans("Height"), 0.5f));

}

void ControllerEmu::LoadDefaults(const ControllerInterface &ciface)
{
	// load an empty inifile section, clears everything
	IniFile::Section sec;
	LoadConfig(&sec);

	if (ciface.Devices().size())
	{
		default_device.FromDevice(ciface.Devices()[0]);
		UpdateDefaultDevice();
	}
}
