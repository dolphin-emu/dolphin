// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifdef _WIN32
	#include <Windows.h>
	#include "InputCommon/ControllerInterface/Sixense/SixenseHack.h"
#endif
#include "Common/Common.h"
#include "Common/Logging/LogManager.h"
#include "InputCommon/ControllerInterface/Sixense/RazerHydra.h"

namespace RazerHydra
{

bool getAccel(int index, bool sideways, bool has_extension, float* gx, float* gy, float* gz)
{
#ifdef _WIN32
	const int left = 0, right = 1;
	if (index == 0 && HydraUpdate() && g_hydra.c[right].enabled && !g_hydra.c[right].docked)
	{
		// World-space accelerations need to be converted into accelerations relative to the Wiimote's sensor.
		float rel_acc[3];
		for (int i = 0; i < 3; ++i)
		{
			rel_acc[i] = g_hydra_state[right].a[0] * g_hydra.c[right].rotation_matrix[i][0]
				+ g_hydra_state[right].a[1] * g_hydra.c[right].rotation_matrix[i][1]
				+ g_hydra_state[right].a[2] * g_hydra.c[right].rotation_matrix[i][2];
		}

		// Note that here gX means to the CONTROLLER'S left, gY means to the CONTROLLER'S tail, and gZ means to the CONTROLLER'S top! 
		// Tilt sensing.
		// If the left Hydra is docked, or an extension is plugged in then just
		// hold the right Hydra sideways yourself. Otherwise in sideways mode 
		// with no extension pitch is controlled by the angle between the hydras.
		if (sideways &&
			!has_extension &&
			g_hydra.c[left].enabled && !g_hydra.c[left].docked)
		{
			// angle between the hydras
			float x = g_hydra.c[right].position[0] - g_hydra.c[left].position[0];
			float y = g_hydra.c[right].position[1] - g_hydra.c[left].position[1];
			float z = g_hydra.c[right].position[2] - g_hydra.c[left].position[2];
			float dist = sqrtf(x*x + y*y + z*z);
			if (dist > 0)
			{
				x = x / dist;
				y = y / dist;
				z = z / dist;
			}
			else
			{
				x = 1;
				y = 0;
				z = 0;
			}
			*gy = y;
			float tail_up = g_hydra.c[right].rotation_matrix[2][1];
			float stick_up = g_hydra.c[right].rotation_matrix[1][1];
			float len = sqrtf(tail_up*tail_up + stick_up*stick_up);
			if (len == 0)
			{
				// neither the tail or the stick is up, the side is up
				*gx = 0;
				*gz = 0;
			}
			else
			{
				if (dist > 0)
				{
					float horiz_dist = sqrtf(x*x + z*z);
					*gx = horiz_dist * tail_up / len;
					*gz = horiz_dist * stick_up / len;
				}
			}

			// Convert rel acc from m/s/s to G's, and to sideways Wiimote's coordinate system.
			*gx += rel_acc[2] / 9.8f;
			*gz += rel_acc[1] / 9.8f;
			*gy -= rel_acc[0] / 9.8f;
		}
		else
		{
			// Tilt sensing.
			*gx = -g_hydra.c[right].rotation_matrix[0][1];
			*gz = g_hydra.c[right].rotation_matrix[1][1];
			*gy = g_hydra.c[right].rotation_matrix[2][1];

			// Convert rel acc from m/s/s to G's, and to Wiimote's coordinate system.
			*gx -= rel_acc[0] / 9.8f;
			*gz += rel_acc[1] / 9.8f;
			*gy += rel_acc[2] / 9.8f;
		}
		return true;
	}
#endif
	return false;
}

bool getButtons(int index, bool sideways, bool has_extension, u32* mask, bool* cycle_extension)
{
	*mask = 0;
	*cycle_extension = false;
#ifdef _WIN32
	// Razer Hydra fixed button mapping
	// START = A, RT/RB = B, 1 = 1, 2 = 2, 3 = -, 4 = +, stick = DPad/Home
	const int left = 0, right = 1;
	if (index == 0 && HydraUpdate() && g_hydra.c[right].enabled)
	{
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_START)
		{
			*mask |= UDPWM_BA;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_1)
		{
			*mask |= UDPWM_B1;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_2)
		{
			*mask |= UDPWM_B2;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_3)
		{
			*mask |= UDPWM_BM;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_4)
		{
			*mask |= UDPWM_BP;
		}
		if ((g_hydra.c[right].buttons & HYDRA_BUTTON_BUMPER) || (g_hydra.c[right].trigger > 0.25))
		{
			*mask |= UDPWM_BB;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_STICK)
		{
			*mask |= UDPWM_BH;
			// In the IR section this button also recenters the IR pointer.
			// That makes sense because the Home screen is guaranteed to have a pointer,
			// is easy to get out of by pressing Home again, and is rarely pressed.
		}

		// If the left Hydra is docked, or an extension is plugged in then just
		// hold the right Hydra sideways yourself. Otherwise in sideways mode 
		// with no extension use the left hydra for DPad, A, and B.
		if (sideways &&
			!has_extension &&
			g_hydra.c[left].enabled && !g_hydra.c[left].docked)
		{
			if (g_hydra_state[left].jx > 0.5f || g_hydra_state[right].jx > 0.5f)
			{
				*mask |= UDPWM_BD;
			}
			else if (g_hydra_state[left].jx < -0.5f || g_hydra_state[right].jx < -0.5f)
			{
				*mask |= UDPWM_BU;
			}
			if (g_hydra_state[left].jy > 0.5f || g_hydra_state[right].jy > 0.5f)
			{
				*mask |= UDPWM_BR;
			}
			else if (g_hydra_state[left].jy < -0.5f || g_hydra_state[right].jy < -0.5f)
			{
				*mask |= UDPWM_BL;
			}
			if ((g_hydra.c[left].buttons & HYDRA_BUTTON_BUMPER) || (g_hydra.c[left].trigger > 0.25))
			{
				*mask |= UDPWM_BB;
			}
			if (g_hydra.c[left].buttons & HYDRA_BUTTON_START)
			{
				*mask |= UDPWM_BA;
			}
		}
		else
		{
			if (g_hydra_state[right].jx > 0.5f)
			{
				*mask |= UDPWM_BR;
			}
			else if (g_hydra_state[right].jx < -0.5f)
			{
				*mask |= UDPWM_BL;
			}
			if (g_hydra_state[right].jy > 0.5f)
			{
				*mask |= UDPWM_BU;
			}
			else if (g_hydra_state[right].jy < -0.5f)
			{
				*mask |= UDPWM_BD;
			}
		}
		// Left hydra stick cycles through extensions: Wiimote, Sideways, Nunchuk, Classic
		if (g_hydra_state[left].released & HYDRA_BUTTON_STICK)
		{
			g_hydra_state[left].released &= ~HYDRA_BUTTON_STICK;
			*cycle_extension = true;
		}
		return true;
	}
#endif
	return false;
}

void getIR(int index, double* x, double* y, double* z)
{
#ifdef _WIN32
	// VR Sixense Razer Hydra
	// Use right Hydra's position as IR pointer.
	// These are in millimetres right, up, and into screen from the base orb.
	static float hydra_ir_center_x = -300.0f, hydra_ir_center_y = -30.0f, hydra_ir_center_z = -300;
	const int left = 0, right = 1;
	if (index == 0 && HydraUpdate() &&
		g_hydra.c[right].enabled && !g_hydra.c[right].docked)
	{
		// The home button (right stick in) also recenters the IR. 
		if (g_hydra_state[right].pressed & HYDRA_BUTTON_STICK)
		{
			g_hydra_state[right].pressed &= ~HYDRA_BUTTON_STICK;
			hydra_ir_center_x = g_hydra.c[right].position[0];
			hydra_ir_center_y = g_hydra.c[right].position[1];
			hydra_ir_center_z = -g_hydra.c[right].position[2];
			NOTICE_LOG(VR, "Razer Hydra IR centre set to: %5.1fcm right, %5.1fcm up, %5.1fcm in", hydra_ir_center_x / 10.0f, hydra_ir_center_y / 10.0f, hydra_ir_center_z / 10.0f);
		}
		*x = (g_hydra.c[right].position[0] - hydra_ir_center_x) / 150;
		*y = (g_hydra.c[right].position[1] - hydra_ir_center_y) / 150;
		*z = (-g_hydra.c[right].position[2] - hydra_ir_center_z) / 300;
	}
#endif
}

bool getNunchuk(int index, float* jx, float* jy, u8* mask)
{
	*mask = 0;
#ifdef _WIN32
	// VR Sixense Razer hydra support
	// Left controller will be nunchuck: stick=stick, LB (or 1)=C, LT (or 2)=Z
	const int left = 0, right = 1;
	if (index == 0 && HydraUpdate() && g_hydra.c[left].enabled && !g_hydra.c[left].docked)
	{
		if ((g_hydra.c[left].buttons & HYDRA_BUTTON_BUMPER) || (g_hydra.c[left].buttons & HYDRA_BUTTON_1))
		{
			*mask |= UDPWM_NC;
		}
		if (g_hydra.c[left].trigger > 0.25f || (g_hydra.c[left].buttons & HYDRA_BUTTON_2))
		{
			*mask |= UDPWM_NZ;
		}
		*jx = g_hydra_state[left].jx;
		*jy = g_hydra_state[left].jy;
		return true;
	}
#endif
	return false;
}

bool getNunchuckAccel(int index, float* gx, float* gy, float* gz)
{
#ifdef _WIN32
	// VR Sixense Razer hydra support
	// Left controller will be nunchuck: stick=stick, LB (or 1)=C, LT (or 2)=Z
	const int left = 0, right = 1;
	if (index==0 && HydraUpdate() && g_hydra.c[left].enabled && !g_hydra.c[left].docked)
	{
		// Note that here X means to the CONTROLLER'S left, Y means to the CONTROLLER'S tail, and Z means to the CONTROLLER'S top! 
		// Tilt sensing.
		*gx = -g_hydra.c[left].rotation_matrix[0][1];
		*gz = g_hydra.c[left].rotation_matrix[1][1];
		*gy = g_hydra.c[left].rotation_matrix[2][1];

		// World-space accelerations need to be converted into accelerations relative to the Wiimote's sensor.
		float rel_acc[3];
		for (int i = 0; i < 3; ++i)
		{
			rel_acc[i] = g_hydra_state[left].a[0] * g_hydra.c[left].rotation_matrix[i][0]
				+ g_hydra_state[left].a[1] * g_hydra.c[left].rotation_matrix[i][1]
				+ g_hydra_state[left].a[2] * g_hydra.c[left].rotation_matrix[i][2];
		}

		// Convert from metres per second per second to G's, and to Wiimote's coordinate system.
		*gx -= rel_acc[0] / 9.8f;
		*gz += rel_acc[1] / 9.8f;
		*gy += rel_acc[2] / 9.8f;
		return true;
	}
#endif
	return false;
}

bool getClassic(int index, float* lx, float* ly, float* rx, float* ry, float* l, float* r, u32* mask)
{
	*mask = 0;
#ifdef _WIN32
	const int left = 0, right = 1;
	if (index == 0 && HydraUpdate() && g_hydra.c[left].enabled && !g_hydra.c[left].docked)
	{
		// L is analog so must be Trigger, therefore ZL is left bumper.
		// Unfortunately this is the opposite layout from the Classic Controller Pro.
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_BUMPER)
		{
			*mask |= CC_B_ZL;
		}
		// The hydra doesn't have a DPad, so use the buttons on the left controller.
		// Imagine the controllers are tilted inwards like a Classic Controller Pro.
		// Therefore 3 is at the top.
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_3)
		{
			*mask |= CC_B_UP;
		}
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_2)
		{
			*mask |= CC_B_DOWN;
		}
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_1)
		{
			*mask |= CC_B_LEFT;
		}
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_4)
		{
			*mask |= CC_B_RIGHT;
		}
		// Left Start = - button
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_START)
		{
			*mask |= CC_B_MINUS;
		}
		// Left analog stick
		*lx = g_hydra_state[left].jx;
		*ly = g_hydra_state[left].jy;
		// Left analog trigger = L
		*l = g_hydra.c[left].trigger;
		if (g_hydra.c[left].trigger > 0.9)
		{
			*mask |= CC_B_L;
		}
		// Right controller
		if (g_hydra.c[right].enabled && !g_hydra.c[right].docked)
		{
			// Right analog stick
			*rx = g_hydra_state[right].jx;
			*ry = g_hydra_state[right].jy;
			// Right analog trigger = R
			*r = g_hydra.c[right].trigger;
			if (g_hydra.c[right].trigger > 0.9)
			{
				*mask |= CC_B_R;
			}
		}
		else
		{
			*rx = 0.0f;
			*ry = 0.0f;
			*r  = 0.0f;
		}
		// Right stick in = Home button
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_STICK)
		{
			*mask |= CC_B_HOME;
		}
		// Right Start = + button
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_START)
		{
			*mask |= CC_B_PLUS;
		}
		// Imagine controllers are tilted inwards like holding a Classic Controller Pro.
		// Therefore 1 = b, 2 = a, 3 = y, 4 = x
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_1)
		{
			*mask |= CC_B_B;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_2)
		{
			*mask |= CC_B_A;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_3)
		{
			*mask |= CC_B_Y;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_4)
		{
			*mask |= CC_B_X;
		}
		// R is analog so must be Trigger, therefore ZR is right bumper.
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_BUMPER)
		{
			*mask |= CC_B_ZR;
		}
		return true;
	}
#endif
	return false;
}

bool getGameCube(int index, float* lx, float* ly, float* rx, float* ry, float* l, float* r, u32* mask)
{
	*mask = 0;
#ifdef _WIN32
	// VR Sixense Razer hydra support
	if (index==0 && HydraUpdate() && g_hydra.c[0].enabled && !g_hydra.c[0].docked)
	{
		const int left = 0, right = 1;
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_BUMPER || g_hydra.c[right].buttons & HYDRA_BUTTON_BUMPER)
		{
			*mask |= GC_B_Z;
		}
		// The hydra doesn't have a DPad, so use the buttons on the left controller.
		// Imagine the controllers are tilted inwards.
		// Therefore 3 is at the top.
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_3)
		{
			*mask |= GC_B_UP;
		}
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_2)
		{
			*mask |= GC_B_DOWN;
		}
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_1)
		{
			*mask |= GC_B_LEFT;
		}
		if (g_hydra.c[left].buttons & HYDRA_BUTTON_4)
		{
			*mask |= GC_B_RIGHT;
		}
		// Left analog stick
		*lx = g_hydra_state[left].jx;
		*ly = g_hydra_state[left].jy;
		// Left analog trigger = L
		*l = g_hydra.c[left].trigger;
		if (g_hydra.c[left].trigger > 0.8)
		{
			*mask |= GC_B_L;
		}
		// Right controller
		if (g_hydra.c[right].enabled && !g_hydra.c[right].docked)
		{
			// Right analog stick
			*rx = g_hydra_state[right].jx;
			*ry = g_hydra_state[right].jy;
			// Right analog trigger = R
			*r = g_hydra.c[right].trigger;
			if (g_hydra.c[right].trigger > 0.8)
			{
				*mask |= GC_B_R;
			}
		}
		else
		{
			*rx = 0.0f;
			*ry = 0.0f;
			*r = 0.0f;
		}
		// Right Start = START/Pause button
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_START)
		{
			*mask |= GC_B_START;
		}
		// 1 = b, 2 = a, 3 = y, 4 = x
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_1)
		{
			*mask |= GC_B_B;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_2)
		{
			*mask |= GC_B_A;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_3)
		{
			*mask |= GC_B_Y;
		}
		if (g_hydra.c[right].buttons & HYDRA_BUTTON_4)
		{
			*mask |= GC_B_X;
		}
		return true;
	}
#endif
	return false;
}

};
