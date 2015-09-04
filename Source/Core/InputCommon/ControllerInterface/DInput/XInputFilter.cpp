// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

// This function is contained in a separate file because WbemIdl.h pulls in files which break on
// /Zc:strictStrings, so this compilation unit is compiled without /Zc:strictStrings.

#include <cstdio>
#include <vector>
#include <WbemIdl.h>
#include <Windows.h>

#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=nullptr; } }

namespace ciface
{
namespace DInput
{

//-----------------------------------------------------------------------------
// Modified some MSDN code to get all the XInput device GUID.Data1 values in a vector,
// faster than checking all the devices for each DirectInput device, like MSDN says to do
//-----------------------------------------------------------------------------
void GetXInputGUIDS(std::vector<DWORD>* guids)
{
	IWbemLocator*           pIWbemLocator  = nullptr;
	IEnumWbemClassObject*   pEnumDevices   = nullptr;
	IWbemClassObject*       pDevices[20]   = {0};
	IWbemServices*          pIWbemServices = nullptr;
	BSTR                    bstrNamespace  = nullptr;
	BSTR                    bstrDeviceID   = nullptr;
	BSTR                    bstrClassName  = nullptr;
	DWORD                   uReturned      = 0;
	VARIANT                 var;
	HRESULT                 hr;

	// CoInit if needed
	hr = CoInitialize(nullptr);
	bool bCleanupCOM = SUCCEEDED(hr);

	// Create WMI
	hr = CoCreateInstance(__uuidof(WbemLocator),
	                      nullptr,
	                      CLSCTX_INPROC_SERVER,
	                      __uuidof(IWbemLocator),
	                      (LPVOID*) &pIWbemLocator);
	if (FAILED(hr) || pIWbemLocator == nullptr)
		goto LCleanup;

	bstrNamespace = SysAllocString(L"\\\\.\\root\\cimv2"); if (bstrNamespace == nullptr) goto LCleanup;
	bstrClassName = SysAllocString(L"Win32_PNPEntity");    if (bstrClassName == nullptr) goto LCleanup;
	bstrDeviceID  = SysAllocString(L"DeviceID");           if (bstrDeviceID == nullptr)  goto LCleanup;

	// Connect to WMI
	hr = pIWbemLocator->ConnectServer(bstrNamespace, nullptr, nullptr, 0L, 0L, nullptr, nullptr, &pIWbemServices);
	if (FAILED(hr) || pIWbemServices == nullptr)
		goto LCleanup;

	// Switch security level to IMPERSONATE.
	CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
	                  RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

	hr = pIWbemServices->CreateInstanceEnum(bstrClassName, 0, nullptr, &pEnumDevices);
	if (FAILED(hr) || pEnumDevices == nullptr)
		goto LCleanup;

	// Loop over all devices
	while (true)
	{
		// Get 20 at a time
		hr = pEnumDevices->Next(10000, 20, pDevices, &uReturned);
		if (FAILED(hr) || uReturned == 0)
			break;

		for (UINT iDevice = 0; iDevice < uReturned; ++iDevice)
		{
			// For each device, get its device ID
			hr = pDevices[iDevice]->Get(bstrDeviceID, 0L, &var, nullptr, nullptr);
			if (SUCCEEDED(hr) && var.vt == VT_BSTR && var.bstrVal != nullptr)
			{
				// Check if the device ID contains "IG_".  If it does, then it's an XInput device
				// This information can not be found from DirectInput
				if (wcsstr(var.bstrVal, L"IG_"))
				{
					// If it does, then get the VID/PID from var.bstrVal
					DWORD dwPid = 0, dwVid = 0;
					WCHAR* strVid = wcsstr(var.bstrVal, L"VID_");
					if (strVid && swscanf(strVid, L"VID_%4X", &dwVid) != 1)
						dwVid = 0;
					WCHAR* strPid = wcsstr(var.bstrVal, L"PID_");
					if (strPid && swscanf(strPid, L"PID_%4X", &dwPid) != 1)
						dwPid = 0;

					// Compare the VID/PID to the DInput device
					DWORD dwVidPid = MAKELONG(dwVid, dwPid);
					guids->push_back(dwVidPid);
					//bIsXinputDevice = true;
				}
			}
			SAFE_RELEASE(pDevices[iDevice]);
		}
	}

LCleanup:
	if (bstrNamespace)
		SysFreeString(bstrNamespace);
	if (bstrDeviceID)
		SysFreeString(bstrDeviceID);
	if (bstrClassName)
		SysFreeString(bstrClassName);
	for (UINT iDevice = 0; iDevice < 20; iDevice++)
		SAFE_RELEASE(pDevices[iDevice]);
	SAFE_RELEASE(pEnumDevices);
	SAFE_RELEASE(pIWbemLocator);
	SAFE_RELEASE(pIWbemServices);

	if (bCleanupCOM)
		CoUninitialize();
}

}
}

#undef SAFE_RELEASE
