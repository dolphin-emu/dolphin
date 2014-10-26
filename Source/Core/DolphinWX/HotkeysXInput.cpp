// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "DolphinWX/Frame.h"
#include "DolphinWX/HotkeysXInput.h"
#include "InputCommon/ControllerInterface/Xinput/Xinput.h"

#define XINPUT_ANALOG_THRESHOLD 0.30

namespace HotkeysXInput
{

	void Update(){
		ciface::Core::Device* xinput_dev = nullptr;

		//Find XInput device
		for (ciface::Core::Device* d : g_controller_interface.Devices())
		{
			// for now, assume first XInput device is the one we want
			if (d->GetSource() == "XInput")
			{
				xinput_dev = d;
				break;
			}
		}

		// get inputs from xinput device
		std::vector<ciface::Core::Device::Input*>::const_iterator
			i = xinput_dev->Inputs().begin(),
			e = xinput_dev->Inputs().end();

		u16 button_states;
		bool binary_trigger_l;
		bool binary_trigger_r;
		bool binary_axis_l_x_pos;
		bool binary_axis_l_x_neg;
		bool binary_axis_l_y_pos;
		bool binary_axis_l_y_neg;
		bool binary_axis_r_x_pos;
		bool binary_axis_r_x_neg;
		bool binary_axis_r_y_pos;
		bool binary_axis_r_y_neg;

		for (i; i != e; ++i)
		{
			std::string control_name = (*i)->GetName();

			if (control_name == "Button A"){
				button_states = (*i)->GetStates();
			}
			else if (control_name == "Trigger L"){
				binary_trigger_l = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Trigger R"){
				binary_trigger_r = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Left X+"){
				binary_axis_l_x_pos = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Left X-"){
				binary_axis_l_x_neg = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Left Y+"){
				binary_axis_l_y_pos = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Left Y-"){
				binary_axis_l_y_neg = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Right X+"){
				binary_axis_r_x_pos = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Right X-"){
				binary_axis_r_x_neg = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Right Y+"){
				binary_axis_r_y_pos = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
			else if (control_name == "Right Y-"){
				binary_axis_r_y_neg = ((*i)->GetState() > XINPUT_ANALOG_THRESHOLD);
			}
		}

		u32 full_controller_state =
			(binary_axis_l_x_neg << 25) |
			(binary_axis_l_x_pos << 24) |
			(binary_axis_l_y_neg << 23) |
			(binary_axis_l_y_pos << 22) |
			(binary_axis_r_x_neg << 21) |
			(binary_axis_r_x_pos << 20) |
			(binary_axis_r_y_neg << 19) |
			(binary_axis_r_y_pos << 18) |
			(binary_trigger_l << 17) |
			(binary_trigger_r << 16) |
			button_states;

		CFrame::OnXInputPoll(&full_controller_state);
	}

}