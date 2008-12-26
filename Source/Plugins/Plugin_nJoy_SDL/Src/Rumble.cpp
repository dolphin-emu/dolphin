//////////////////////////////////////////////////////////////////////////////////////////
// Project description
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003-2008 Dolphin Project.
//
//////////////////////////////////////////////////////////////////////////////////////////
//
// Licensetype: GNU General Public License (GPL)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.
//
// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/
//
// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/
//
//////////////////////////////////////////////////////////////////////////////////////////


////////////////////////
// Include
// ¯¯¯¯¯¯¯¯¯
#include "nJoy.h"


//////////////////////////////////////////////////////////////////////////////////////////
// Enable or disable rumble. Set USE_RUMBLE_DINPUT_HACK in nJoy.h
// ¯¯¯¯¯¯¯¯¯
#ifdef USE_RUMBLE_DINPUT_HACK
bool g_rumbleEnable = FALSE;
#endif

// Rumble in windows
#ifdef _WIN32

	#ifdef USE_RUMBLE_DINPUT_HACK
	LPDIRECTINPUT8          g_pDI = NULL;
	LPDIRECTINPUTDEVICE8    g_pDevice = NULL;
	LPDIRECTINPUTEFFECT     g_pEffect = NULL;

	DWORD                   g_dwNumForceFeedbackAxis = 0;
	INT                     g_nXForce = 0;
	INT                     g_nYForce = 0;

	#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }

	HRESULT InitDirectInput(HWND hDlg);
	//VOID FreeDirectInput();
	BOOL CALLBACK EnumFFDevicesCallback(const DIDEVICEINSTANCE* pInst, VOID* pContext);
	BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext);
	HRESULT SetDeviceForcesXY();
	#endif

#elif defined(__linux__)
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>

	int fd;
	char device_file_name[64];
	struct ff_effect effect;
	bool CanRumble = false;
#endif
//////////////////////



// Set PAD rumble. Explanation: Stop = 0, Rumble = 1
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	//if (_numPAD > 0)
	//	return;

	// SDL can't rumble the gamepad so we need to use platform specific code
	#ifdef _WIN32
	#ifdef USE_RUMBLE_DINPUT_HACK
	static int a = 0;

	if ((_uType == 0) || (_uType == 2))
	{
		a = 0;
	}
	else if (_uType == 1)
	{
		a = _uStrength > 2 ? 8000 : 0;
	}

	a = int ((float)a * 0.96f);

	if (!g_rumbleEnable)
	{
		a = 0;		
	}
	else
	{
		g_nYForce = a;
		SetDeviceForcesXY();
	}
	#endif
	#elif defined(__linux__)
	struct input_event event;
	if (CanRumble)
	{
		if (_uType == 1)
		{
			event.type = EV_FF;
			event.code = effect.id;
			event.value = 1;
			if (write(fd, (const void*) &event, sizeof(event)) == -1) {
				perror("Play effect");
				exit(1);
			}
		}
		if ((_uType == 0) || (_uType == 2))
		{
			event.type = EV_FF;
			event.code =  effect.id;
			event.value = 0;
			if (write(fd, (const void*) &event, sizeof(event)) == -1) {
				perror("Stop effect");
				exit(1);
			}
		}
	}
	#endif
}



// Use PAD rumble
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯
void PAD_Use_Rumble(u8 _numPAD, SPADStatus* _pPADStatus)
{
	#ifdef _WIN32
	#ifdef USE_RUMBLE_DINPUT_HACK

	// Enable or disable rumble
	if (joystate[_numPAD].halfpress)
	if (!g_pDI)
	if (FAILED(InitDirectInput(m_hWnd)))
	{
		MessageBox(NULL, SDL_GetError(), "Could not initialize DirectInput!", MB_ICONERROR);
		g_rumbleEnable = FALSE;
		//return;
	}
	else
	{
		g_rumbleEnable = TRUE;
	}

	if (g_rumbleEnable)
	{
		g_pDevice->Acquire();
		
		if (g_pEffect) g_pEffect->Start(1, 0);
	}
	#endif
	#elif defined(__linux__)
	if (!fd)
	{
		sprintf(device_file_name, "/dev/input/event%d", joysticks[_numPAD].eventnum); //TODO: Make dynamic //

		/* Open device */
		fd = open(device_file_name, O_RDWR);
		if (fd == -1) {
			perror("Open device file");
			//Something wrong, probably permissions, just return now
			return;
		}
		int n_effects = 0;
		if (ioctl(fd, EVIOCGEFFECTS, &n_effects) == -1) {
			perror("Ioctl number of effects");
		}
		if (n_effects > 0)
			CanRumble = true;
		else
			return; // Return since we can't do any effects
		/* a strong rumbling effect */
		effect.type = FF_RUMBLE;
		effect.id = -1;
		effect.u.rumble.strong_magnitude = 0x8000;
		effect.u.rumble.weak_magnitude = 0;
		effect.replay.length = 5000; // Set to 5 seconds, if a Game needs more for a single rumble event, it is dumb and must be a demo
		effect.replay.delay = 0;
		if (ioctl(fd, EVIOCSFF, &effect) == -1) {
			perror("Upload effect");
			CanRumble = false; //We have effects but it doesn't support the rumble we are using. This is basic rumble, should work for most
		}
	}
	#endif
}






#ifdef _WIN32
//////////////////////////////////////////////////////////////////////////////////////////
// Rumble stuff :D!
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
//
#ifdef USE_RUMBLE_DINPUT_HACK
HRESULT InitDirectInput( HWND hDlg )
{
    DIPROPDWORD dipdw;
    HRESULT hr;

    // Register with the DirectInput subsystem and get a pointer to a IDirectInput interface we can use.
    if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&g_pDI, NULL)))
    {
        return hr;
    }

    // Look for a force feedback device we can use
    if (FAILED(hr = g_pDI->EnumDevices( DI8DEVCLASS_GAMECTRL, EnumFFDevicesCallback, NULL, DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK)))
    {
        return hr;
    }

    if (NULL == g_pDevice)
    {
        MessageBox(NULL, "Force feedback device not found. nJoy will now disable rumble." ,"FFConst" , MB_ICONERROR | MB_OK);
		g_rumbleEnable = FALSE;
        
        return S_OK;
    }

    // Set the data format to "simple joystick" - a predefined data format. A
    // data format specifies which controls on a device we are interested in,
    // and how they should be reported.
    //
    // This tells DirectInput that we will be passing a DIJOYSTATE structure to
    // IDirectInputDevice8::GetDeviceState(). Even though we won't actually do
    // it in this sample. But setting the data format is important so that the
    // DIJOFS_* values work properly.
    if (FAILED(hr = g_pDevice->SetDataFormat(&c_dfDIJoystick)))
        return hr;

    // Set the cooperative level to let DInput know how this device should
    // interact with the system and with other DInput applications.
    // Exclusive access is required in order to perform force feedback.
    //if (FAILED(hr = g_pDevice->SetCooperativeLevel(hDlg, DISCL_EXCLUSIVE | DISCL_FOREGROUND)))

	if (FAILED(hr = g_pDevice->SetCooperativeLevel(hDlg, DISCL_EXCLUSIVE | DISCL_FOREGROUND)))	
    {
        return hr;
    }

    // Since we will be playing force feedback effects, we should disable the
    // auto-centering spring.
    dipdw.diph.dwSize = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj = 0;
    dipdw.diph.dwHow = DIPH_DEVICE;
    dipdw.dwData = FALSE;

    if (FAILED(hr = g_pDevice->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph)))
        return hr;

    // Enumerate and count the axes of the joystick 
    if (FAILED(hr = g_pDevice->EnumObjects(EnumAxesCallback, (VOID*)&g_dwNumForceFeedbackAxis, DIDFT_AXIS)))
        return hr;

    // This simple sample only supports one or two axis joysticks
    if (g_dwNumForceFeedbackAxis > 2)
        g_dwNumForceFeedbackAxis = 2;

    // This application needs only one effect: Applying raw forces.
    DWORD rgdwAxes[2] = {DIJOFS_X, DIJOFS_Y};
    LONG rglDirection[2] = {0, 0};
    DICONSTANTFORCE cf = {0};

    DIEFFECT eff;
    ZeroMemory(&eff, sizeof(eff));
    eff.dwSize = sizeof(DIEFFECT);
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.dwDuration = INFINITE;
    eff.dwSamplePeriod = 0;
    eff.dwGain = DI_FFNOMINALMAX;
    eff.dwTriggerButton = DIEB_NOTRIGGER;
    eff.dwTriggerRepeatInterval = 0;
    eff.cAxes = g_dwNumForceFeedbackAxis;
    eff.rgdwAxes = rgdwAxes;
    eff.rglDirection = rglDirection;
    eff.lpEnvelope = 0;
    eff.cbTypeSpecificParams = sizeof( DICONSTANTFORCE );
    eff.lpvTypeSpecificParams = &cf;
    eff.dwStartDelay = 0;

    // Create the prepared effect
    if (FAILED(hr = g_pDevice->CreateEffect(GUID_ConstantForce, &eff, &g_pEffect, NULL)))
    {
        return hr;
    }

    if (NULL == g_pEffect)
        return E_FAIL;

    return S_OK;
}

VOID FreeDirectInput()
{
    // Unacquire the device one last time just in case 
    // the app tried to exit while the device is still acquired.
    if (g_pDevice)
        g_pDevice->Unacquire();

    // Release any DirectInput objects.
    SAFE_RELEASE(g_pEffect);
    SAFE_RELEASE(g_pDevice);
    SAFE_RELEASE(g_pDI);
}

BOOL CALLBACK EnumFFDevicesCallback( const DIDEVICEINSTANCE* pInst, VOID* pContext )
{
    LPDIRECTINPUTDEVICE8 pDevice;
    HRESULT hr;

    // Obtain an interface to the enumerated force feedback device.
    hr = g_pDI->CreateDevice(pInst->guidInstance, &pDevice, NULL);

    // If it failed, then we can't use this device for some bizarre reason.  
	// (Maybe the user unplugged it while we were in the middle of enumerating it.)  So continue enumerating
    if (FAILED(hr))
        return DIENUM_CONTINUE;

    // We successfully created an IDirectInputDevice8.  So stop looking for another one.
    g_pDevice = pDevice;

    return DIENUM_STOP;
}

BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext)
{
    DWORD* pdwNumForceFeedbackAxis = (DWORD*)pContext;
    if ((pdidoi->dwFlags & DIDOI_FFACTUATOR) != 0)
        (*pdwNumForceFeedbackAxis)++;

    return DIENUM_CONTINUE;
}

HRESULT SetDeviceForcesXY()
{
    // Modifying an effect is basically the same as creating a new one, except you need only specify the parameters you are modifying
    LONG rglDirection[2] = { 0, 0 };

    DICONSTANTFORCE cf;

    if (g_dwNumForceFeedbackAxis == 1)
    {
        // If only one force feedback axis, then apply only one direction and keep the direction at zero
        cf.lMagnitude = g_nXForce;
        rglDirection[0] = 0;
    }
    else
    {
        // If two force feedback axis, then apply magnitude from both directions 
        rglDirection[0] = g_nXForce;
        rglDirection[1] = g_nYForce;
        cf.lMagnitude = (DWORD)sqrt((double)g_nXForce * (double)g_nXForce + (double)g_nYForce * (double)g_nYForce );
    }

    DIEFFECT eff;
    ZeroMemory(&eff, sizeof(eff));
    eff.dwSize = sizeof(DIEFFECT);
    eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
    eff.cAxes = g_dwNumForceFeedbackAxis;
    eff.rglDirection = rglDirection;
    eff.lpEnvelope = 0;
    eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
    eff.lpvTypeSpecificParams = &cf;
    eff.dwStartDelay = 0;

    // Now set the new parameters and start the effect immediately.
    return g_pEffect->SetParameters(&eff, DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_START);
}
#endif
#endif
