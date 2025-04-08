// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Host.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/USB/Emulated/MotionCamera.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{

const u8 usb_config_desc[] = {
	0x09, 0x02, 0x09, 0x03, 0x02, 0x01, 0x30, 0x80, 0xFA, 0x08, 0x0B, 0x00,
	0x02, 0x0E, 0x03, 0x00, 0x60, 0x09, 0x04, 0x00, 0x00, 0x01, 0x0E, 0x01,
	0x00, 0x60, 0x0D, 0x24, 0x01, 0x00, 0x01, 0x4D, 0x00, 0xC0, 0xE1, 0xE4,
	0x00, 0x01, 0x01, 0x09, 0x24, 0x03, 0x02, 0x01, 0x01, 0x00, 0x04, 0x00,
	0x1A, 0x24, 0x06, 0x04, 0xF0, 0x77, 0x35, 0xD1, 0x89, 0x8D, 0x00, 0x47,
	0x81, 0x2E, 0x7D, 0xD5, 0xE2, 0xFD, 0xB8, 0x98, 0x08, 0x01, 0x03, 0x01,
	0xFF, 0x00, 0x12, 0x24, 0x02, 0x01, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x03,
                             // 0x0A, 0x02, 0x00, // patch bmControls to avoid unnecessary requests
                                0x00, 0x00, 0x00,
                                                  0x0B, 0x24, 0x05, 0x03,
	0x01, 0x00, 0x00, 0x02,
                       // 0x7F, 0x15, // patch bmControls to avoid unnecessary requests
                          0x00, 0x00,
                                      0x00, 0x07, 0x05, 0x82, 0x03, 0x10,
	0x00, 0x06, 0x05, 0x25, 0x03, 0x10, 0x00, 0x09, 0x04, 0x01, 0x00, 0x00,
	0x0E, 0x02, 0x00, 0x00, 0x0F, 0x24, 0x01, 0x02, 0x2D, 0x02, 0x81, 0x00,
	0x02, 0x02, 0x01, 0x00, 0x01, 0x00, 0x00, 0x0B, 0x24, 0x06, 0x01, 0x05,
	0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x26, 0x24, 0x07, 0x01, 0x00, 0x80,
	0x02, 0xE0, 0x01, 0x00, 0xF4, 0x01, 0x00, 0x00, 0xC0, 0xA8, 0x00, 0x00,
	0x08, 0x07, 0x00, 0x15, 0x16, 0x05, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00,
	0x76, 0x96, 0x98, 0x00, 0x15, 0x16, 0x05, 0x00, 0x26, 0x24, 0x07, 0x02,
	0x00, 0x40, 0x01, 0xF0, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00, 0x30, 0x2A,
	0x00, 0x00, 0xC2, 0x01, 0x00, 0x15, 0x16, 0x05, 0x00, 0x00, 0x15, 0x16,
	0x05, 0x00, 0x76, 0x96, 0x98, 0x00, 0x15, 0x16, 0x05, 0x00, 0x26, 0x24,
	0x07, 0x03, 0x00, 0xA0, 0x00, 0x78, 0x00, 0x00, 0xF4, 0x01, 0x00, 0x00,
	0x8C, 0x0A, 0x00, 0x80, 0x70, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x00,
	0x15, 0x16, 0x05, 0x00, 0x76, 0x96, 0x98, 0x00, 0x15, 0x16, 0x05, 0x00,
	0x26, 0x24, 0x07, 0x04, 0x00, 0xB0, 0x00, 0x90, 0x00, 0x00, 0xF4, 0x01,
	0x00, 0x00, 0xEC, 0x0D, 0x00, 0x80, 0x94, 0x00, 0x00, 0x15, 0x16, 0x05,
	0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x76, 0x96, 0x98, 0x00, 0x15, 0x16,
	0x05, 0x00, 0x26, 0x24, 0x07, 0x05, 0x00, 0x60, 0x01, 0x20, 0x01, 0x00,
	0xF4, 0x01, 0x00, 0x00, 0xB0, 0x37, 0x00, 0x00, 0x52, 0x02, 0x00, 0x15,
	0x16, 0x05, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x76, 0x96, 0x98, 0x00,
	0x15, 0x16, 0x05, 0x00, 0x1A, 0x24, 0x03, 0x00, 0x05, 0x80, 0x02, 0xE0,
	0x01, 0x40, 0x01, 0xF0, 0x00, 0xA0, 0x00, 0x78, 0x00, 0xB0, 0x00, 0x90,
	0x00, 0x60, 0x01, 0x20, 0x01, 0x00, 0x06, 0x24, 0x0D, 0x01, 0x01, 0x04,
	0x1B, 0x24, 0x04, 0x02, 0x05, 0x59, 0x55, 0x59, 0x32, 0x00, 0x00, 0x10,
	0x00, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71, 0x10, 0x01, 0x00,
	0x00, 0x00, 0x00, 0x32, 0x24, 0x05, 0x01, 0x00, 0x80, 0x02, 0xE0, 0x01,
	0x00, 0x60, 0x09, 0x00, 0x00, 0x40, 0x19, 0x01, 0x00, 0x60, 0x09, 0x00,
	0x15, 0x16, 0x05, 0x00, 0x06, 0x15, 0x16, 0x05, 0x00, 0x20, 0xA1, 0x07,
	0x00, 0x2A, 0x2C, 0x0A, 0x00, 0x40, 0x42, 0x0F, 0x00, 0x80, 0x84, 0x1E,
	0x00, 0x80, 0x96, 0x98, 0x00, 0x32, 0x24, 0x05, 0x02, 0x00, 0x40, 0x01,
	0xF0, 0x00, 0x00, 0x58, 0x02, 0x00, 0x00, 0x50, 0x46, 0x00, 0x00, 0x58,
	0x02, 0x00, 0x15, 0x16, 0x05, 0x00, 0x06, 0x15, 0x16, 0x05, 0x00, 0x20,
	0xA1, 0x07, 0x00, 0x2A, 0x2C, 0x0A, 0x00, 0x40, 0x42, 0x0F, 0x00, 0x80,
	0x84, 0x0F, 0x00, 0x80, 0x96, 0x98, 0x00, 0x32, 0x24, 0x05, 0x03, 0x00,
	0xA0, 0x00, 0x78, 0x00, 0x00, 0x96, 0x00, 0x00, 0x00, 0x94, 0x11, 0x00,
	0x00, 0x96, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x06, 0x15, 0x16, 0x05,
	0x00, 0x20, 0xA1, 0x07, 0x00, 0x2A, 0x2C, 0x0A, 0x00, 0x40, 0x42, 0x0F,
	0x00, 0x80, 0x84, 0x0F, 0x00, 0x80, 0x96, 0x98, 0x00, 0x32, 0x24, 0x05,
	0x04, 0x00, 0xB0, 0x00, 0x90, 0x00, 0x00, 0xC6, 0x00, 0x00, 0x00, 0x34,
	0x17, 0x00, 0x00, 0xC6, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x06, 0x15,
	0x16, 0x05, 0x00, 0x20, 0xA1, 0x07, 0x00, 0x2A, 0x2C, 0x0A, 0x00, 0x40,
	0x42, 0x0F, 0x00, 0x80, 0x84, 0x0F, 0x00, 0x80, 0x96, 0x98, 0x00, 0x32,
	0x24, 0x05, 0x05, 0x00, 0x60, 0x01, 0x20, 0x01, 0x00, 0x18, 0x03, 0x00,
	0x00, 0xD0, 0x5C, 0x00, 0x00, 0x18, 0x03, 0x00, 0x15, 0x16, 0x05, 0x00,
	0x06, 0x15, 0x16, 0x05, 0x00, 0x20, 0xA1, 0x07, 0x00, 0x2A, 0x2C, 0x0A,
	0x00, 0x40, 0x42, 0x0F, 0x00, 0x80, 0x84, 0x0F, 0x00, 0x80, 0x96, 0x98,
	0x00, 0x1A, 0x24, 0x03, 0x00, 0x05, 0x80, 0x02, 0xE0, 0x01, 0x40, 0x01,
	0xF0, 0x00, 0xA0, 0x00, 0x78, 0x00, 0xB0, 0x00, 0x90, 0x00, 0x60, 0x01,
	0x20, 0x01, 0x00, 0x06, 0x24, 0x0D, 0x01, 0x01, 0x04, 0x09, 0x04, 0x01,
	0x01, 0x01, 0x0E, 0x02, 0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0x60, 0x0A,
	0x01, 0x09, 0x04, 0x01, 0x02, 0x01, 0x0E, 0x02, 0x00, 0x00, 0x07, 0x05,
	0x81, 0x05, 0x00, 0x0B, 0x01, 0x09, 0x04, 0x01, 0x03, 0x01, 0x0E, 0x02,
	0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0x20, 0x0B, 0x01, 0x09, 0x04, 0x01,
	0x04, 0x01, 0x0E, 0x02, 0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0x00, 0x13,
	0x01, 0x09, 0x04, 0x01, 0x05, 0x01, 0x0E, 0x02, 0x00, 0x00, 0x07, 0x05,
	0x81, 0x05, 0x20, 0x13, 0x01, 0x09, 0x04, 0x01, 0x06, 0x01, 0x0E, 0x02,
	0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0xFC, 0x13, 0x01
};

DeviceDescriptor MotionCamera::s_device_descriptor{
  .bLength            = 0x12,
  .bDescriptorType    = 0x01,
  .bcdUSB             = 0x0200,
  .bDeviceClass       = 0xef,
  .bDeviceSubClass    = 0x02,
  .bDeviceProtocol    = 0x01,
  .bMaxPacketSize0    = 0x40,
  .idVendor           = 0x057e,
  .idProduct          = 0x030a,
  .bcdDevice          = 0x0924,
  .iManufacturer      = 0x30,
  .iProduct           = 0x60,
  .iSerialNumber      = 0x00,
  .bNumConfigurations = 0x01
};
std::vector<ConfigDescriptor> MotionCamera::s_config_descriptor{
  {
    .bLength             = 0x09,
    .bDescriptorType     = 0x02,
    .wTotalLength        = 0x0309,
    .bNumInterfaces      = 0x02,
    .bConfigurationValue = 0x01,
    .iConfiguration      = 0x30,
    .bmAttributes        = 0x80,
    .MaxPower            = 0xfa,
  }
};
std::vector<InterfaceDescriptor> MotionCamera::s_interface_descriptor{
  {
    .bLength            = 0x09,
    .bDescriptorType    = 0x04,
    .bInterfaceNumber   = 0x00,
    .bAlternateSetting  = 0x00,
    .bNumEndpoints      = 0x01,
    .bInterfaceClass    = 0x0e,
    .bInterfaceSubClass = 0x01,
    .bInterfaceProtocol = 0x00,
    .iInterface         = 0x60,
  },
  {
    .bLength            = 0x09,
    .bDescriptorType    = 0x04,
    .bInterfaceNumber   = 0x01,
    .bAlternateSetting  = 0x00,
    .bNumEndpoints      = 0x00,
    .bInterfaceClass    = 0x0e,
    .bInterfaceSubClass = 0x02,
    .bInterfaceProtocol = 0x00,
    .iInterface         = 0x00,
  },
  {
    .bLength            = 0x09,
    .bDescriptorType    = 0x04,
    .bInterfaceNumber   = 0x01,
    .bAlternateSetting  = 0x01,
    .bNumEndpoints      = 0x01,
    .bInterfaceClass    = 0x0e,
    .bInterfaceSubClass = 0x02,
    .bInterfaceProtocol = 0x00,
    .iInterface         = 0x00,
  },
  {
    .bLength            = 0x09,
    .bDescriptorType    = 0x04,
    .bInterfaceNumber   = 0x01,
    .bAlternateSetting  = 0x02,
    .bNumEndpoints      = 0x01,
    .bInterfaceClass    = 0x0e,
    .bInterfaceSubClass = 0x02,
    .bInterfaceProtocol = 0x00,
    .iInterface         = 0x00,
  },
  {
    .bLength            = 0x09,
    .bDescriptorType    = 0x04,
    .bInterfaceNumber   = 0x01,
    .bAlternateSetting  = 0x03,
    .bNumEndpoints      = 0x01,
    .bInterfaceClass    = 0x0e,
    .bInterfaceSubClass = 0x02,
    .bInterfaceProtocol = 0x00,
    .iInterface         = 0x00,
  },
  {
    .bLength            = 0x09,
    .bDescriptorType    = 0x04,
    .bInterfaceNumber   = 0x01,
    .bAlternateSetting  = 0x04,
    .bNumEndpoints      = 0x01,
    .bInterfaceClass    = 0x0e,
    .bInterfaceSubClass = 0x02,
    .bInterfaceProtocol = 0x00,
    .iInterface         = 0x00,
  },
  {
    .bLength            = 0x09,
    .bDescriptorType    = 0x04,
    .bInterfaceNumber   = 0x01,
    .bAlternateSetting  = 0x05,
    .bNumEndpoints      = 0x01,
    .bInterfaceClass    = 0x0e,
    .bInterfaceSubClass = 0x02,
    .bInterfaceProtocol = 0x00,
    .iInterface         = 0x00,
  },
  {
    .bLength            = 0x09,
    .bDescriptorType    = 0x04,
    .bInterfaceNumber   = 0x01,
    .bAlternateSetting  = 0x06,
    .bNumEndpoints      = 0x01,
    .bInterfaceClass    = 0x0e,
    .bInterfaceSubClass = 0x02,
    .bInterfaceProtocol = 0x00,
    .iInterface         = 0x00,
  }
};
std::vector<EndpointDescriptor> MotionCamera::s_endpoint_descriptor{
  {
    .bLength          = 0x07,
    .bDescriptorType  = 0x05,
    .bEndpointAddress = 0x82,
    .bmAttributes     = 0x03,
    .wMaxPacketSize   = 0x0010,
    .bInterval        = 0x06,
  },
  {
    .bLength          = 0x07,
    .bDescriptorType  = 0x05,
    .bEndpointAddress = 0x81,
    .bmAttributes     = 0x05,
    .wMaxPacketSize   = 0x0a60,
    .bInterval        = 0x01,
  },
  {
    .bLength          = 0x07,
    .bDescriptorType  = 0x05,
    .bEndpointAddress = 0x81,
    .bmAttributes     = 0x05,
    .wMaxPacketSize   = 0x0b00,
    .bInterval        = 0x01,
  },
  {
    .bLength          = 0x07,
    .bDescriptorType  = 0x05,
    .bEndpointAddress = 0x81,
    .bmAttributes     = 0x05,
    .wMaxPacketSize   = 0x0b20,
    .bInterval        = 0x01,
  },
  {
    .bLength          = 0x07,
    .bDescriptorType  = 0x05,
    .bEndpointAddress = 0x81,
    .bmAttributes     = 0x05,
    .wMaxPacketSize   = 0x1300,
    .bInterval        = 0x01,
  },
  {
    .bLength          = 0x07,
    .bDescriptorType  = 0x05,
    .bEndpointAddress = 0x81,
    .bmAttributes     = 0x05,
    .wMaxPacketSize   = 0x1320,
    .bInterval        = 0x01,
  },
  {
    .bLength          = 0x07,
    .bDescriptorType  = 0x05,
    .bEndpointAddress = 0x81,
    .bmAttributes     = 0x05,
    .wMaxPacketSize   = 0x13fc,
    .bInterval        = 0x01,
  }
};

std::string getUVCVideoStreamingControl(u8 value)
{
  std::string names[] = { "VS_CONTROL_UNDEFINED", "VS_PROBE", "VS_COMMIT" };
  if (value <= VS_COMMIT)
    return names[value];
  return "Unknown";
}

std::string getUVCRequest(u8 value)
{
  if (value == SET_CUR)
    return "SET_CUR";
  std::string names[] = { "GET_CUR", "GET_MIN", "GET_MAX", "GET_RES", "GET_LEN", "GET_INF", "GET_DEF" };
  if (GET_CUR <= value && value <= GET_DEF)
    return names[value - GET_CUR];
  return "Unknown";
}

std::string getUVCTerminalControl(u8 value)
{
  std::string names[] = { "CONTROL_UNDEFINED", "SCANNING_MODE", "AE_MODE", "AE_PRIORITY", "EXPOSURE_TIME_ABSOLUTE",
    "EXPOSURE_TIME_RELATIVE", "FOCUS_ABSOLUTE", "FOCUS_RELATIVE", "FOCUS_AUTO", "IRIS_ABSOLUTE", "IRIS_RELATIVE",
    "ZOOM_ABSOLUTE", "ZOOM_RELATIVE", "PANTILT_ABSOLUTE", "PANTILT_RELATIVE", "ROLL_ABSOLUTE", "ROLL_RELATIVE",
    "PRIVACY" };
  if (value <= CT_PRIVACY)
    return names[value];
  return "Unknown";
}

std::string getUVCProcessingUnitControl(u8 value)
{
  std::string names[] = { "CONTROL_UNDEFINED", "BACKLIGHT_COMPENSATION", "BRIGHTNESS", "CONTRAST", "GAIN",
    "POWER_LINE_FREQUENCY", "HUE", "SATURATION", "SHARPNESS", "GAMMA", "WHITE_BALANCE_TEMPERATURE",
    "WHITE_BALANCE_TEMPERATURE_AUTO", "WHITE_BALANCE_COMPONENT", "WHITE_BALANCE_COMPONENT_AUTO", "DIGITAL_MULTIPLIER",
    "DIGITAL_MULTIPLIER_LIMIT", "HUE_AUTO", "ANALOG_VIDEO_STANDARD", "ANALOG_LOCK_STATUS" };
  if (value <= PU_ANALOG_LOCK_STATUS)
    return names[value];
  return "Unknown";
}

MotionCamera::MotionCamera(EmulationKernel& ios) : m_ios(ios)
{
  m_id = (u64(m_vid) << 32 | u64(m_pid) << 16 | u64(9) << 8 | u64(1));

  m_active_size = {320, 240};
  m_image_size = m_active_size.width * m_active_size.height * 2;
  m_image_pos  = 0;
  m_image_data = (u8*) calloc(1, m_image_size);
  m_frame_id = 0;
}

MotionCamera::~MotionCamera() {
  if (m_active_altsetting)
  {
    NOTICE_LOG_FMT(IOS_USB, "Host_CameraStop");
    Host_CameraStop();
  }
};

DeviceDescriptor MotionCamera::GetDeviceDescriptor() const
{
  return s_device_descriptor;
}

std::vector<ConfigDescriptor> MotionCamera::GetConfigurations() const
{
  return s_config_descriptor;
}

std::vector<InterfaceDescriptor> MotionCamera::GetInterfaces(u8 config) const
{
  return s_interface_descriptor;
}

std::vector<EndpointDescriptor> MotionCamera::GetEndpoints(u8 config, u8 interface, u8 alt) const
{
  std::vector<EndpointDescriptor> ret;
  if (interface == 0)
    ret.push_back(s_endpoint_descriptor[0]);
  else if (interface == 1 && alt > 0)
    ret.push_back(s_endpoint_descriptor[alt]);
  return ret;
}

bool MotionCamera::Attach()
{
  NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening device", m_vid, m_pid);
  return true;
}

bool MotionCamera::AttachAndChangeInterface(const u8 interface)
{
  if (!Attach())
    return false;

  if (interface != m_active_interface)
    return ChangeInterface(interface) == 0;

  return true;
}

int MotionCamera::CancelTransfer(const u8 endpoint)
{
  INFO_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Cancelling transfers (endpoint {:#x})", m_vid, m_pid,
               m_active_interface, endpoint);
  return IPC_SUCCESS;
}

int MotionCamera::ChangeInterface(const u8 interface)
{
  NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Changing interface to {}", m_vid, m_pid, m_active_interface, interface);
  m_active_interface = interface;
  return 0;
}

int MotionCamera::GetNumberOfAltSettings(u8 interface)
{
  NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] GetNumberOfAltSettings: interface={:02x}", m_vid, m_pid, interface);
  return (interface == 1) ? 7 : 1;
}

int MotionCamera::SetAltSetting(u8 alt_setting)
{
  NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] SetAltSetting: alt_setting={:02x}", m_vid, m_pid, alt_setting);
  m_active_altsetting = alt_setting;
  if (alt_setting)
  {
    NOTICE_LOG_FMT(IOS_USB, "Host_CameraStart({}x{})", m_active_size.width, m_active_size.height);
    Host_CameraStart(m_active_size.width, m_active_size.height);
  }
  else
  {
    NOTICE_LOG_FMT(IOS_USB, "Host_CameraStop");
    Host_CameraStop();
  }
  return 0;
}

int MotionCamera::SubmitTransfer(std::unique_ptr<CtrlMessage> cmd)
{
  switch ((cmd->request_type << 8) | cmd->request)
  {
    case USBHDR(DIR_DEVICE2HOST, TYPE_STANDARD, REC_DEVICE, REQUEST_GET_DESCRIPTOR): // 0x80 0x06
    {
      std::vector<u8> control_response(usb_config_desc, usb_config_desc + sizeof(usb_config_desc));
      ScheduleTransfer(std::move(cmd), control_response, 0);
      break;
    }
    case USBHDR(DIR_HOST2DEVICE, TYPE_CLASS, REC_INTERFACE, SET_CUR): // 0x21 0x01
    {
      u8 unit = cmd->index >> 8;
      u8 control = cmd->value >> 8;
      NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x} wIndex={:04x} wLength={:04x} // {} / {}",
                               m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value, cmd->index, cmd->length,
                               getUVCRequest(cmd->request),
                               (unit == 0) ? getUVCVideoStreamingControl(control)
                               : (unit == 1) ? getUVCTerminalControl(control)
                               : (unit == 3) ? getUVCProcessingUnitControl(control)
                               : "");
      if (unit == 0 && control == VS_COMMIT)
      {
        auto& system = m_ios.GetSystem();
        auto& memory = system.GetMemory();
        UVCProbeCommitControl* commit = (UVCProbeCommitControl*) memory.GetPointerForRange(cmd->data_address, cmd->length);
        m_active_size = m_supported_sizes[commit->bFrameIndex - 1];
        NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] VS_COMMIT: bFormatIndex={:02x} bFrameIndex={:02x} dwFrameInterval={:04x} new format {}x{}",
                                 m_vid, m_pid, commit->bFormatIndex, commit->bFrameIndex, commit->dwFrameInterval,
                                 m_active_size.width, m_active_size.height
                                 );
      }
      std::vector<u8> control_response = {};
      ScheduleTransfer(std::move(cmd), control_response, 0);
      break;
    }
    case USBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, GET_CUR): // 0xa1 0x81
    case USBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, GET_MIN): // 0xa1 0x82
    case USBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, GET_MAX): // 0xa1 0x83
    case USBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, GET_RES): // 0xa1 0x84
    case USBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, GET_LEN): // 0xa1 0x85
    case USBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, GET_INF): // 0xa1 0x86
    case USBHDR(DIR_DEVICE2HOST, TYPE_CLASS, REC_INTERFACE, GET_DEF): // 0xa1 0x87
    {
      u8 unit = cmd->index >> 8;
      NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x} wIndex={:04x} wLength={:04x} // {} / {}",
                               m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value, cmd->index, cmd->length,
                               getUVCRequest(cmd->request),
                               (unit == 0) ? getUVCVideoStreamingControl(cmd->value >> 8)
                               : (unit == 1) ? getUVCTerminalControl(cmd->value >> 8)
                               : (unit == 3) ? getUVCProcessingUnitControl(cmd->value >> 8)
                               : "");
      std::vector<u8> control_response = {};
      ScheduleTransfer(std::move(cmd), control_response, 0);
      break;
    }
    default:
    {
      NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Control: bRequestType={:02x} bRequest={:02x} wValue={:04x} wIndex={:04x} wLength={:04x}",
                    m_vid, m_pid, m_active_interface, cmd->request_type, cmd->request, cmd->value, cmd->index, cmd->length);
      std::vector<u8> control_response = {};
      ScheduleTransfer(std::move(cmd), control_response, 0);
    }
  }
  return IPC_SUCCESS;
}

int MotionCamera::SubmitTransfer(std::unique_ptr<BulkMessage> cmd)
{
  NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Bulk: length={:04x} endpoint={:02x}", m_vid, m_pid,
                m_active_interface, cmd->length, cmd->endpoint);
  return IPC_SUCCESS;
}

int MotionCamera::SubmitTransfer(std::unique_ptr<IntrMessage> cmd)
{
  NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x} {}] Interrupt: length={:04x} endpoint={:02x}", m_vid,
                m_pid, m_active_interface, cmd->length, cmd->endpoint);
  return IPC_SUCCESS;
}

int MotionCamera::SubmitTransfer(std::unique_ptr<IsoMessage> cmd)
{
  auto& system = m_ios.GetSystem();
  auto& memory = system.GetMemory();
  u8* iso_buffer = memory.GetPointerForRange(cmd->data_address, cmd->length);
  if (!iso_buffer)
  {
    ERROR_LOG_FMT(IOS_USB, "MotionCamera iso buf error");
    return IPC_EINVAL;
  }

  u8* iso_buffer_pos = iso_buffer;

  for (std::size_t i = 0; i < cmd->num_packets; i++)
  {
    UVCHeader uvc_header{};
    uvc_header.bHeaderLength = sizeof(UVCHeader);
    uvc_header.endOfHeader = 1;
    uvc_header.frameId = m_frame_id;

    u32 data_size = (m_image_size - m_image_pos > cmd->packet_sizes[i] - sizeof(uvc_header))
      ? cmd->packet_sizes[i] - sizeof(uvc_header)
      : m_image_size - m_image_pos;
    if (data_size > 0 && m_image_pos + data_size == m_image_size)
    {
      m_frame_id ^= 1;
      uvc_header.endOfFrame = 1;
    }
    std::memcpy(iso_buffer_pos, &uvc_header, sizeof(uvc_header));
    if (data_size > 0)
    {
      std::memcpy(iso_buffer_pos + sizeof(uvc_header), m_image_data + m_image_pos, data_size);
    }
    m_image_pos += data_size;
    iso_buffer_pos += sizeof(uvc_header) + data_size;
    cmd->SetPacketReturnValue(i, (u32)sizeof(uvc_header) + data_size);
  }

  if (m_image_pos == m_image_size)
  {
    system.GetCameraData().GetData(m_image_data, m_image_size);
    m_image_pos = 0;
  }

  // 15 fps, one frame every 66ms, half a frame per transfer, one transfer every 33ms
  cmd->ScheduleTransferCompletion(IPC_SUCCESS, 33000);
  return IPC_SUCCESS;
}

void MotionCamera::ScheduleTransfer(std::unique_ptr<TransferCommand> command,
                                   const std::vector<u8>& data, u64 expected_time_us)
{
  command->FillBuffer(data.data(), static_cast<const size_t>(data.size()));
  command->ScheduleTransferCompletion(static_cast<s32>(data.size()), expected_time_us);
}

CameraData::CameraData()
{
  int w = 320;
  int h = 240;
  m_image_size = 0;
  m_image_data = (u8*) calloc(1, w * h * 2);

  for (int line = 0 ; line < h; line++) {
    for (int col = 0; col < w; col++) {
      u8 *pos = m_image_data + 2 * (w * line + col);

      u8 r = col  * 255 / w;
      u8 g = col  * 255 / w;
      u8 b = line * 255 / h;

      u8 y = (( 66 * r + 129 * g +  25 * b + 128) / 256) +  16;
      u8 u = ((-38 * r -  74 * g + 112 * b + 128) / 256) + 128;
      u8 v = ((112 * r -  94 * g -  18 * b + 128) / 256) + 128;

      pos[0] = y;
      pos[1] = (col % 2 == 0) ? u : v;
    }
  }
}

void CameraData::SetData(const u8* data, u32 length)
{
  m_image_size = length;
  memcpy(m_image_data, data, length);
}

void CameraData::GetData(const u8* data, u32 length)
{
  memcpy((void*)data, m_image_data, length);
}
}  // namespace IOS::HLE::USB
