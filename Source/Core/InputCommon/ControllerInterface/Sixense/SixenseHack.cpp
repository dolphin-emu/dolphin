#include <Windows.h>
#include "Common/Common.h"
#include "Common/Logging/Log.h"
#include "InputCommon/ControllerInterface/Sixense/SixenseHack.h"

typedef int(__cdecl *PHydra_Init)();
typedef int(__cdecl *PHydra_Exit)();
typedef int(__cdecl *PHydra_GetAllNewestData)(TAllHydraControllers *all_controllers);

PHydra_Init Hydra_Init = nullptr;
PHydra_Exit Hydra_Exit = nullptr;
PHydra_GetAllNewestData Hydra_GetAllNewestData = nullptr;

static HMODULE g_sixense_lib = nullptr;

bool g_sixense_initialized = false;
static THydraControllerState g_old[2];
THydraControllerState g_hydra_state[2];
static TAllHydraControllers g_oldhydra;
TAllHydraControllers g_hydra;

void InitSixenseLib()
{
	if (!g_sixense_initialized)
	{
		if (!g_sixense_lib)
		{
#ifdef _WIN64
			g_sixense_lib = LoadLibrary(_T("sixense_x64.dll"));
#else
			g_sixense_lib = LoadLibrary(_T("sixense.dll"));
#endif

			if (!g_sixense_lib)
			{
#ifdef _WIN64
				WARN_LOG(VR, "Failed to load sixense_x64.dll. Razer Hydra and Sixense STEM will not be supported.");
#else
				WARN_LOG(VR, "Failed to load sixense.dll. Razer Hydra and Sixense STEM will not be supported.");
#endif
				return;
			}
		}

		Hydra_Init = (PHydra_Init)GetProcAddress(g_sixense_lib, "sixenseInit");
		Hydra_Exit = (PHydra_Exit)GetProcAddress(g_sixense_lib, "sixenseExit");
		Hydra_GetAllNewestData = (PHydra_GetAllNewestData)GetProcAddress(g_sixense_lib, "sixenseGetAllNewestData");
		if (!Hydra_Init || !Hydra_GetAllNewestData)
		{
			ERROR_LOG(VR, "Failed to load minimum needed sixense dll functions. Razer Hydra and Sixense STEM will not be supported.");
			return;
		}
		if (Hydra_Init()!=0)
		{
			ERROR_LOG(VR, "Hydra_Init() function failed. Razer Hydra and Sixense STEM will not be supported.");
			return;
		}
		ZeroMemory(g_hydra_state, 2*sizeof(g_hydra_state[0]));
		ZeroMemory(g_old, 2 * sizeof(g_old[0]));
		ZeroMemory(&g_hydra, sizeof(g_hydra));
		ZeroMemory(&g_oldhydra, sizeof(g_oldhydra));
		HydraUpdate();

		g_sixense_initialized = true;
		NOTICE_LOG(VR, "Sixense Razer Hydra driver initialized.");
	}
}

void HydraUpdateController(int hand)
{
	// buttons which have changed state to pressed (but haven't been read yet)
	g_hydra_state[hand].pressed |= g_hydra.c[hand].buttons & ~g_oldhydra.c[hand].buttons;
	g_hydra_state[hand].released |= g_oldhydra.c[hand].buttons & ~g_hydra.c[hand].buttons;
	// Get time since last update. Razer Hydra is locked to 60Hz.
	s16 frames = (s16)g_hydra.c[hand].sequence_number - (s16)g_oldhydra.c[hand].sequence_number;
	if (frames < 0)
	{
		frames += 256;
	}
	float t = frames/60.0f;
	for (int axis = 0; axis < 3; ++axis)
	{
		g_hydra_state[hand].t = t;
		// get smoothed position
		g_hydra_state[hand].p[axis] = (g_hydra.c[hand].position[axis] + g_oldhydra.c[hand].position[axis]) / 2000;
		// get velocity
		g_hydra_state[hand].v[axis] = (g_hydra_state[hand].p[axis] - g_old[hand].p[axis]) / t;
		// get acceleration
		g_hydra_state[hand].a[axis] = (g_hydra_state[hand].v[axis] - g_old[hand].v[axis]) / t;
	}
	// As soon as we remove hydra from its dock, recalibrate joystick centre (if possible).
	if (g_oldhydra.c[hand].docked && !g_hydra.c[hand].docked)
	{
		if (fabs(g_hydra.c[hand].stick_x) < 1.0f && fabs(g_hydra.c[hand].stick_y) < 1.0f)
		{
			g_hydra_state[hand].jcx = g_hydra.c[hand].stick_x;
			g_hydra_state[hand].jcy = g_hydra.c[hand].stick_y;
			NOTICE_LOG(VR, "Hydra %d analog stick recalibrated to (%5.2f, %5.2f)", hand, g_hydra_state[hand].jcx, g_hydra_state[hand].jcy);
			MessageBeep(MB_ICONASTERISK);
		}
		else
		{
			ERROR_LOG(VR, "Unable to recalibrate Hydra %d because joypos=(%5.2f, %5.2f)", hand, g_hydra.c[hand].stick_x, g_hydra.c[hand].stick_y);
			MessageBeep(0xFFFFFFFF);
		}
	}
	// Return calibrated joystick values.
	if (g_hydra.c[hand].stick_x <= g_hydra_state[hand].jcx)
	{
		g_hydra_state[hand].jx = (g_hydra.c[hand].stick_x - g_hydra_state[hand].jcx) / (g_hydra_state[hand].jcx - -1.0f);
	}
	else
	{
		g_hydra_state[hand].jx = (g_hydra.c[hand].stick_x - g_hydra_state[hand].jcx) / (1.0f - g_hydra_state[hand].jcx);
	}
	if (g_hydra.c[hand].stick_y <= g_hydra_state[hand].jcy)
	{
		g_hydra_state[hand].jy = (g_hydra.c[hand].stick_y - g_hydra_state[hand].jcy) / (g_hydra_state[hand].jcy - -1.0f);
	}
	else
	{
		g_hydra_state[hand].jy = (g_hydra.c[hand].stick_y - g_hydra_state[hand].jcy) / (1.0f - g_hydra_state[hand].jcy);
	}
	// Save current state as old state for next time.
	g_old[hand] = g_hydra_state[hand];
}

bool HydraUpdate()
{
	if (!g_sixense_initialized || Hydra_GetAllNewestData(&g_hydra) != 0)
	{
		return false;
	}
	bool changed = false;
	for (int i = 0; i < 2; ++i)
	{
		if (g_hydra.c[i].enabled && g_hydra.c[i].sequence_number != g_oldhydra.c[i].sequence_number)
		{
			changed = true;
			HydraUpdateController(i);
		}
	}
	if (changed)
	{
		g_oldhydra = g_hydra;
	}
	return true;
}