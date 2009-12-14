
// Project description
// -------------------
// Name: nJoy 
// Description: A Dolphin Compatible Input Plugin
//
// Author: Falcon4ever (nJoy@falcon4ever.com)
// Site: www.multigesture.net
// Copyright (C) 2003 Dolphin Project.
//

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

// Include
// ---------
#include "nJoy.h"


#ifdef RUMBLE_HACK

struct RUMBLE // GC Pad rumble DIDevice 
{	
	LPDIRECTINPUTDEVICE8	g_pDevice;	// 4 pads objects
	LPDIRECTINPUTEFFECT		g_pEffect;
	DWORD					g_dwNumForceFeedbackAxis;
	DIEFFECT				eff;
};

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=NULL; } }

BOOL CALLBACK EnumFFDevicesCallback(const DIDEVICEINSTANCE* pInst, VOID* pContext);
BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext);
void SetDeviceForcesXY(int pad, int nXYForce);
HRESULT InitRumble(HWND hWnd);

LPDIRECTINPUT8		g_Rumble;		// DInput Rumble object
RUMBLE				pRumble[4];		// 4 GC Rumble Pads

//////////////////////
// Use PAD rumble
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯

void Pad_Use_Rumble(u8 _numPAD)
{
	if (PadMapping[_numPAD].rumble)
	{
		if (!g_Rumble)
		{
			// GetForegroundWindow() always sends the good HWND
			if (FAILED(InitRumble(GetForegroundWindow())))
				PanicAlert("Could not initialize Rumble!");
		} else
		{
			// Acquire gamepad
			if (pRumble[_numPAD].g_pDevice != NULL)
				pRumble[_numPAD].g_pDevice->Acquire();
		}
	}
}

////////////////////////////////////////////////////
// Set PAD rumble. Explanation: Stop = 0, Rumble = 1
// ¯¯¯¯¯¯¯¯¯¯¯¯¯¯

void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	if (!PadMapping[_numPAD].enable)
		return;

	Pad_Use_Rumble(_numPAD);

	int Strenght = 0;

	if (PadMapping[_numPAD].rumble)  // rumble activated
	{
		if (_uType == 1 && _uStrength > 2) 
		{
			// it looks like _uStrength is equal to 3 everytime anyway...
			Strenght = 1000 * (g_Config.RumbleStrength + 1);
			Strenght = Strenght > 10000 ? 10000 : Strenght;
		}
		else
			Strenght = 0;

		SetDeviceForcesXY(_numPAD, Strenght);
	}
}

// Rumble stuff :D!
// ----------------
//

HRESULT InitRumble(HWND hWnd)
{
	DIPROPDWORD dipdw;
	HRESULT hr;

	// Register with the DirectInput subsystem and get a pointer to a IDirectInput interface we can use.
	if (FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&g_Rumble, NULL)))
		return hr;

	// Look for a device we can use
	if (FAILED(hr = g_Rumble->EnumDevices( DI8DEVCLASS_GAMECTRL, EnumFFDevicesCallback, NULL, DIEDFL_ATTACHEDONLY | DIEDFL_FORCEFEEDBACK)))
		return hr;

	for (int i=0; i<4; i++)
	{
		if (NULL == pRumble[i].g_pDevice)
			PadMapping[i].rumble = false; // Disable Rumble for this pad only.
		else
		{
			pRumble[i].g_pDevice->SetDataFormat(&c_dfDIJoystick);
			pRumble[i].g_pDevice->SetCooperativeLevel(hWnd, DISCL_EXCLUSIVE | DISCL_BACKGROUND);
			// Request exclusive acces for both background and foreground.

				dipdw.diph.dwSize = sizeof(DIPROPDWORD);
				dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
				dipdw.diph.dwObj = 0;
				dipdw.diph.dwHow = DIPH_DEVICE;
				dipdw.dwData = FALSE;

			// if Force Feedback doesn't seem to work...
			if (FAILED(pRumble[i].g_pDevice->EnumObjects(EnumAxesCallback, 
				(void*)&pRumble[i].g_dwNumForceFeedbackAxis, DIDFT_AXIS))
			 || FAILED(pRumble[i].g_pDevice->SetProperty(DIPROP_AUTOCENTER, &dipdw.diph)))
			{
				PanicAlert("Device %d doesn't seem to work ! \nRumble for device %d is now Disabled !", i+1);

				PadMapping[i].rumble = false; // Disable Rumble for this pad

				continue;	// Next pad
			}

			if (pRumble[i].g_dwNumForceFeedbackAxis > 2) 
				pRumble[i].g_dwNumForceFeedbackAxis = 2;

			DWORD _rgdwAxes[2] = {DIJOFS_X, DIJOFS_Y};
			long rglDirection[2] = {0, 0};
			DICONSTANTFORCE cf = {0};

			ZeroMemory(&pRumble[i].eff, sizeof(pRumble[i].eff));
			pRumble[i].eff.dwSize = sizeof(DIEFFECT);
			pRumble[i].eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
			pRumble[i].eff.dwDuration = INFINITE; // fixed time may be safer (X * DI_SECONDS)
			pRumble[i].eff.dwSamplePeriod = 0;
			pRumble[i].eff.dwGain = DI_FFNOMINALMAX;
			pRumble[i].eff.dwTriggerButton = DIEB_NOTRIGGER;
			pRumble[i].eff.dwTriggerRepeatInterval = 0;
			pRumble[i].eff.cAxes = pRumble[i].g_dwNumForceFeedbackAxis;
			pRumble[i].eff.rgdwAxes = _rgdwAxes;
			pRumble[i].eff.rglDirection = rglDirection;
			pRumble[i].eff.lpEnvelope = 0;
			pRumble[i].eff.cbTypeSpecificParams = sizeof( DICONSTANTFORCE );
			pRumble[i].eff.lpvTypeSpecificParams = &cf;
			pRumble[i].eff.dwStartDelay = 0;

			// Create the prepared effect
			if (FAILED(hr = pRumble[i].g_pDevice->CreateEffect(GUID_ConstantForce, &pRumble[i].eff, &pRumble[i].g_pEffect, NULL)))
				continue;
					
			if (pRumble[i].g_pEffect == NULL)
				continue;
		}
	}

	return S_OK;
}

void SetDeviceForcesXY(int npad, int nXYForce)
{
	// Security check
	if (pRumble[npad].g_pDevice == NULL)
		return;

	// If nXYForce is null, there's no point to create the effect
	// Just stop the force feedback
	if (nXYForce == 0) {
		pRumble[npad].g_pEffect->Stop();
		return;
	}
	
	long rglDirection[2] = {0};
	DICONSTANTFORCE cf;

	// If only one force feedback axis, then apply only one direction and keep the direction at zero
	if (pRumble[npad].g_dwNumForceFeedbackAxis == 1)
	{
		rglDirection[0] = 0;
		cf.lMagnitude = nXYForce; // max should be 10000
	}
	// 	If two force feedback axis, then apply magnitude from both directions 
	else
	{
		rglDirection[0] = nXYForce;
		rglDirection[1] = nXYForce;
		cf.lMagnitude = 1.4142f*nXYForce;
	}

	ZeroMemory(&pRumble[npad].eff, sizeof(pRumble[npad].eff));
	pRumble[npad].eff.dwSize = sizeof(DIEFFECT);
	pRumble[npad].eff.dwFlags = DIEFF_CARTESIAN | DIEFF_OBJECTOFFSETS;
	pRumble[npad].eff.cAxes = pRumble[npad].g_dwNumForceFeedbackAxis;
	pRumble[npad].eff.rglDirection = rglDirection;
	pRumble[npad].eff.lpEnvelope = 0;
	pRumble[npad].eff.cbTypeSpecificParams = sizeof(DICONSTANTFORCE);
	pRumble[npad].eff.lpvTypeSpecificParams = &cf;
	pRumble[npad].eff.dwStartDelay = 0;

	// Now set the new parameters..
	pRumble[npad].g_pEffect->SetParameters(&pRumble[npad].eff, DIEP_DIRECTION | DIEP_TYPESPECIFICPARAMS | DIEP_START);
	// ..And start the effect immediately.
	if (pRumble[npad].g_pEffect != NULL)
			pRumble[npad].g_pEffect->Start(1, 0);
}

BOOL CALLBACK EnumFFDevicesCallback(const DIDEVICEINSTANCE* pInst, VOID* pContext)
{
	LPDIRECTINPUTDEVICE8 pDevice;
	DIPROPDWORD dipdw;
	HRESULT	hr;

	int JoystickID;

		dipdw.diph.dwSize       = sizeof(DIPROPDWORD); 
		dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER); 
		dipdw.diph.dwObj        = 0; 
		dipdw.diph.dwHow        = DIPH_DEVICE; 

	g_Rumble->CreateDevice(pInst->guidInstance, &pDevice, NULL); // Create a DInput pad device

	if (SUCCEEDED(hr = pDevice->GetProperty(DIPROP_JOYSTICKID, &dipdw.diph))) // Get DInput Device ID 
		JoystickID = dipdw.dwData;
	else 
		return DIENUM_CONTINUE;

	//PanicAlert("DInput ID : %d \nSDL ID (1-4) : %d / %d / %d / %d\n", JoystickID, PadMapping[0].ID, PadMapping[1].ID, PadMapping[2].ID, PadMapping[3].ID);

	for (int i=0; i<4; i++) 
	{
		if (PadMapping[i].ID == JoystickID) // if SDL ID = DInput ID -> we're dealing with the same device
		{
			// a DInput device is created even if rumble is disabled on startup
			// this way, you can toggle the rumble setting while in game
			//if (PadMapping[i].enabled) // && PadMapping[i].rumble
				pRumble[i].g_pDevice = pDevice; // everything looks good, save the DInput device
		}
	}

	return DIENUM_CONTINUE;
}

BOOL CALLBACK EnumAxesCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext)
{
	DWORD* pdwNumForceFeedbackAxis = (DWORD*)pContext;	// Enum Rumble Axis
	if ((pdidoi->dwFlags & DIDOI_FFACTUATOR) != 0)
		(*pdwNumForceFeedbackAxis)++;

	return DIENUM_CONTINUE;
}

void PAD_RumbleClose()
{
    // It may look weird, but we don't free anything here, it was the cause of crashes
	// on stop, and the DLL isn't unloaded anyway, so the pointers stay
	// We just stop the rumble in case it's still playing an effect.
	for (int i=0; i<4; i++)
	{
		if (pRumble[i].g_pDevice && pRumble[i].g_pEffect)
			pRumble[i].g_pEffect->Stop();
	}
}

#else // Multiplatform SDL Rumble code

#ifdef SDL_RUMBLE

struct RUMBLE // GC Pad rumble DIDevice 
{	
	SDL_Haptic*				g_pDevice;
	SDL_HapticEffect		g_pEffect;
	int						effect_id;
};

RUMBLE pRumble[4] = {0};		// 4 GC Rumble Pads
#endif


// Use PAD rumble
// --------------
bool PAD_Init_Rumble(u8 _numPAD, SDL_Joystick *SDL_Device)
{
#ifdef SDL_RUMBLE
	if (SDL_Device == NULL)
		return false;

	pRumble[_numPAD].g_pDevice = SDL_HapticOpenFromJoystick(SDL_Device);

	if (pRumble[_numPAD].g_pDevice == NULL)
		return false; // Most likely joystick isn't haptic	
	
	if (!(SDL_HapticQuery(pRumble[_numPAD].g_pDevice) & SDL_HAPTIC_CONSTANT))
	{
		SDL_HapticClose(pRumble[_numPAD].g_pDevice); // No effect
		pRumble[_numPAD].g_pDevice = 0;
		PadMapping[_numPAD].rumble = false;
		return false;
	}

	// Set the strength of the rumble effect
	int Strenght = 3276 * (g_Config.RumbleStrength + 1);
	Strenght = Strenght > 32767 ? 32767 : Strenght;

	// Create the effect
	memset(&pRumble[_numPAD].g_pEffect, 0, sizeof(SDL_HapticEffect)); // 0 is safe default
	pRumble[_numPAD].g_pEffect.type = SDL_HAPTIC_CONSTANT;
	pRumble[_numPAD].g_pEffect.constant.direction.type = SDL_HAPTIC_POLAR; // Polar coordinates
	pRumble[_numPAD].g_pEffect.constant.direction.dir[0] = 18000; // Force comes from south
	pRumble[_numPAD].g_pEffect.constant.level = Strenght;
	pRumble[_numPAD].g_pEffect.constant.length = 10000; // 10s long (should be INFINITE, but 10s is safer)
	pRumble[_numPAD].g_pEffect.constant.attack_length = 0; // disable Fade in...
	pRumble[_numPAD].g_pEffect.constant.fade_length = 0; // ...and out

	// Upload the effect
	pRumble[_numPAD].effect_id = SDL_HapticNewEffect( pRumble[_numPAD].g_pDevice, &pRumble[_numPAD].g_pEffect );
#endif
	return true;
}


// Set PAD rumble. Explanation: Stop = 0, Rumble = 1
// --------------
void PAD_Rumble(u8 _numPAD, unsigned int _uType, unsigned int _uStrength)
{
	int Strenght = 0;

#ifdef SDL_RUMBLE
	if (PadMapping[_numPAD].rumble)  // rumble activated
	{
		if (!pRumble[_numPAD].g_pDevice)
			return;

		if (_uType == 1 && _uStrength > 2)
			SDL_HapticRunEffect( pRumble[_numPAD].g_pDevice, pRumble[_numPAD].effect_id, 1 );
		else
			SDL_HapticStopAll(pRumble[_numPAD].g_pDevice);
	}
#endif
}

void PAD_RumbleClose()
{
#ifdef SDL_RUMBLE
	for (int i=0; i<4; i++)		// Free all pads
	{
		if (pRumble[i].g_pDevice) {
			SDL_HapticClose( pRumble[i].g_pDevice );
			pRumble[i].g_pDevice = NULL;
		}
	}
#endif
}

#endif // RUMBLE_HACK
