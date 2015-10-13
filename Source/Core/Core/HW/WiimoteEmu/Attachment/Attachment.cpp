// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Common/Common.h"
#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/WiimoteEmu.h"
#include "Core/HW/WiimoteEmu/Attachment/Attachment.h"

namespace WiimoteEmu
{

#if defined(_MSC_VER) && _MSC_VER <= 1800
// Moved to header file when VS supports constexpr.
const ControlState Attachment::DEFAULT_ATTACHMENT_STICK_RADIUS = 1.0f;
#endif

// Extension device IDs to be written to the last bytes of the extension reg
// The id for nothing inserted
static const u8 nothing_id[] = { 0x00, 0x00, 0x00, 0x00, 0x2e, 0x2e };
// The id for a partially inserted extension (currently unused)
UNUSED static const u8 partially_id[] = { 0x00, 0x00, 0x00, 0x00, 0xff, 0xff };

Attachment::Attachment(const char* const _name, WiimoteEmu::ExtensionReg& _reg)
	: name(_name), reg(_reg)
{
	memset(id, 0, sizeof(id));
	memset(calibration, 0, sizeof(calibration));
}

None::None(WiimoteEmu::ExtensionReg& _reg) : Attachment("None", _reg)
{
	// set up register
	memcpy(&id, nothing_id, sizeof(nothing_id));
}

std::string Attachment::GetName() const
{
	return name;
}

void Attachment::Reset()
{
	// set up register
	memset(&reg, 0, WIIMOTE_REG_EXT_SIZE);
	memcpy(&reg.constant_id, id, sizeof(id));
	memcpy(&reg.calibration, calibration, sizeof(calibration));
}

}

void ControllerEmu::Extension::GetState(u8* const data)
{
	((WiimoteEmu::Attachment*)attachments[active_extension].get())->GetState(data);
}

bool ControllerEmu::Extension::IsButtonPressed() const
{
	// Extension == 0 means no Extension, > 0 means one is connected
	// Since we want to use this to know if disconnected Wiimotes want to be connected, and disconnected
	// Wiimotes (can? always?) have their active_extension set to -1, we also have to check the switch_extension
	if (active_extension > 0)
		return ((WiimoteEmu::Attachment*)attachments[active_extension].get())->IsButtonPressed();
	if (switch_extension > 0)
		return ((WiimoteEmu::Attachment*)attachments[switch_extension].get())->IsButtonPressed();
	return false;
}
