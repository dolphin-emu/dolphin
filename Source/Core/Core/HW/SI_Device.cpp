// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <string>

#include "Common/StringUtil.h"
#include "Core/HW/SI_Device.h"
#include "Core/HW/SI_DeviceAMBaseboard.h"
#include "Core/HW/SI_DeviceDanceMat.h"
#include "Core/HW/SI_DeviceGBA.h"
#include "Core/HW/SI_DeviceGCController.h"
#include "Core/HW/SI_DeviceGCSteeringWheel.h"
#include "Core/HW/SI_DeviceKeyboard.h"

// --- interface ISIDevice ---
int ISIDevice::RunBuffer(u8* _pBuffer, int _iLength)
{
#ifdef _DEBUG
	DEBUG_LOG(SERIALINTERFACE, "Send Data Device(%i) - Length(%i)   ", ISIDevice::m_iDeviceNumber, _iLength);

	std::string temp;
	int num = 0;

	while (num < _iLength)
	{
		temp += StringFromFormat("0x%02x ", _pBuffer[num^3]);
		num++;

		if ((num % 8) == 0)
		{
			DEBUG_LOG(SERIALINTERFACE, "%s", temp.c_str());
			temp.clear();
		}
	}

	DEBUG_LOG(SERIALINTERFACE, "%s", temp.c_str());
#endif
	return 0;
}

int ISIDevice::TransferInterval()
{
	return 0;
}

// Stub class for saying nothing is attached, and not having to deal with null pointers :)
class CSIDevice_Null : public ISIDevice
{
public:
	CSIDevice_Null(SIDevices device, int _iDeviceNumber) : ISIDevice(device, _iDeviceNumber) {}
	virtual ~CSIDevice_Null() {}

	int RunBuffer(u8* _pBuffer, int _iLength) override {
		reinterpret_cast<u32*>(_pBuffer)[0] = SI_ERROR_NO_RESPONSE;
		return 4;
	}
	bool GetData(u32& _Hi, u32& _Low) override {
		_Hi = 0x80000000;
		return true;
	}
	void SendCommand(u32 _Cmd, u8 _Poll) override {}
};


// F A C T O R Y
ISIDevice* SIDevice_Create(const SIDevices device, const int port_number)
{
	switch (device)
	{
	case SIDEVICE_GC_CONTROLLER:
		return new CSIDevice_GCController(device, port_number);
		break;

	case SIDEVICE_DANCEMAT:
		return new CSIDevice_DanceMat(device, port_number);
		break;

	case SIDEVICE_GC_STEERING:
		return new CSIDevice_GCSteeringWheel(device, port_number);
		break;

	case SIDEVICE_GC_TARUKONGA:
		return new CSIDevice_TaruKonga(device, port_number);
		break;

	case SIDEVICE_GC_GBA:
		return new CSIDevice_GBA(device, port_number);
		break;

	case SIDEVICE_GC_KEYBOARD:
		return new CSIDevice_Keyboard(device, port_number);
		break;

	case SIDEVICE_AM_BASEBOARD:
		return new CSIDevice_AMBaseboard(device, port_number);
		break;

	case SIDEVICE_NONE:
	default:
		return new CSIDevice_Null(device, port_number);
		break;
	}
}
