// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Host.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/USB/Emulated/MotionCamera.h"
#include "Core/System.h"

namespace IOS::HLE::USB
{

const u8 usb_config_desc[] = {
  0x09, 0x02, 0x09, 0x03, 0x02, 0x01, 0x30, 0x80, 0xfa, 0x08, 0x0b, 0x00, 0x02, 0x0e, 0x03, 0x00,
  0x60, 0x09, 0x04, 0x00, 0x00, 0x01, 0x0e, 0x01, 0x00, 0x60, 0x0d, 0x24, 0x01, 0x00, 0x01, 0x4d,
  0x00, 0xc0, 0xe1, 0xe4, 0x00, 0x01, 0x01, 0x09, 0x24, 0x03, 0x02, 0x01, 0x01, 0x00, 0x04, 0x00,
  0x1a, 0x24, 0x06, 0x04, 0xf0, 0x77, 0x35, 0xd1, 0x89, 0x8d, 0x00, 0x47, 0x81, 0x2e, 0x7d, 0xd5,
  0xe2, 0xfd, 0xb8, 0x98, 0x08, 0x01, 0x03, 0x01, 0xff, 0x00, 0x12, 0x24, 0x02, 0x01, 0x01, 0x02,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
                                                     // 0x0A, 0x02, 0x00, // patch bmControls to avoid unnecessary requests
                                                        0x00, 0x00, 0x00,
                                                                          0x0b, 0x24, 0x05, 0x03,
  0x01, 0x00, 0x00, 0x02,
                       // 0x7F, 0x15, // patch bmControls to avoid unnecessary requests
                          0x00, 0x00,
                                      0x00, 0x07, 0x05, 0x82, 0x03, 0x10, 0x00, 0x06, 0x05, 0x25,
  0x03, 0x10, 0x00, 0x09, 0x04, 0x01, 0x00, 0x00, 0x0e, 0x02, 0x00, 0x00, 0x0f, 0x24, 0x01, 0x02,
  0x2d, 0x02, 0x81, 0x00, 0x02, 0x02, 0x01, 0x00, 0x01, 0x00, 0x00, 0x0b, 0x24, 0x06, 0x01, 0x05,
  0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x26, 0x24, 0x07, 0x01, 0x00, 0x80, 0x02, 0xe0, 0x01, 0x00,
  0xf4, 0x01, 0x00, 0x00, 0xc0, 0xa8, 0x00, 0x00, 0x08, 0x07, 0x00, 0x15, 0x16, 0x05, 0x00, 0x00,
  0x15, 0x16, 0x05, 0x00, 0x76, 0x96, 0x98, 0x00, 0x15, 0x16, 0x05, 0x00, 0x26, 0x24, 0x07, 0x02,
  0x00, 0x40, 0x01, 0xf0, 0x00, 0x00, 0xf4, 0x01, 0x00, 0x00, 0x30, 0x2a, 0x00, 0x00, 0xc2, 0x01,
  0x00, 0x15, 0x16, 0x05, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x76, 0x96, 0x98, 0x00, 0x15, 0x16,
  0x05, 0x00, 0x26, 0x24, 0x07, 0x03, 0x00, 0xa0, 0x00, 0x78, 0x00, 0x00, 0xf4, 0x01, 0x00, 0x00,
  0x8c, 0x0a, 0x00, 0x80, 0x70, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00,
  0x76, 0x96, 0x98, 0x00, 0x15, 0x16, 0x05, 0x00, 0x26, 0x24, 0x07, 0x04, 0x00, 0xb0, 0x00, 0x90,
  0x00, 0x00, 0xf4, 0x01, 0x00, 0x00, 0xec, 0x0d, 0x00, 0x80, 0x94, 0x00, 0x00, 0x15, 0x16, 0x05,
  0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x76, 0x96, 0x98, 0x00, 0x15, 0x16, 0x05, 0x00, 0x26, 0x24,
  0x07, 0x05, 0x00, 0x60, 0x01, 0x20, 0x01, 0x00, 0xf4, 0x01, 0x00, 0x00, 0xb0, 0x37, 0x00, 0x00,
  0x52, 0x02, 0x00, 0x15, 0x16, 0x05, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x76, 0x96, 0x98, 0x00,
  0x15, 0x16, 0x05, 0x00, 0x1a, 0x24, 0x03, 0x00, 0x05, 0x80, 0x02, 0xe0, 0x01, 0x40, 0x01, 0xf0,
  0x00, 0xa0, 0x00, 0x78, 0x00, 0xb0, 0x00, 0x90, 0x00, 0x60, 0x01, 0x20, 0x01, 0x00, 0x06, 0x24,
  0x0d, 0x01, 0x01, 0x04, 0x1b, 0x24, 0x04, 0x02, 0x05, 0x59, 0x55, 0x59, 0x32, 0x00, 0x00, 0x10,
  0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x32,
  0x24, 0x05, 0x01, 0x00, 0x80, 0x02, 0xe0, 0x01, 0x00, 0x60, 0x09, 0x00, 0x00, 0x40, 0x19, 0x01,
  0x00, 0x60, 0x09, 0x00, 0x15, 0x16, 0x05, 0x00, 0x06, 0x15, 0x16, 0x05, 0x00, 0x20, 0xa1, 0x07,
  0x00, 0x2a, 0x2c, 0x0a, 0x00, 0x40, 0x42, 0x0f, 0x00, 0x80, 0x84, 0x1e, 0x00, 0x80, 0x96, 0x98,
  0x00, 0x32, 0x24, 0x05, 0x02, 0x00, 0x40, 0x01, 0xf0, 0x00, 0x00, 0x58, 0x02, 0x00, 0x00, 0x50,
  0x46, 0x00, 0x00, 0x58, 0x02, 0x00, 0x15, 0x16, 0x05, 0x00, 0x06, 0x15, 0x16, 0x05, 0x00, 0x20,
  0xa1, 0x07, 0x00, 0x2a, 0x2c, 0x0a, 0x00, 0x40, 0x42, 0x0f, 0x00, 0x80, 0x84, 0x0f, 0x00, 0x80,
  0x96, 0x98, 0x00, 0x32, 0x24, 0x05, 0x03, 0x00, 0xa0, 0x00, 0x78, 0x00, 0x00, 0x96, 0x00, 0x00,
  0x00, 0x94, 0x11, 0x00, 0x00, 0x96, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x06, 0x15, 0x16, 0x05,
  0x00, 0x20, 0xa1, 0x07, 0x00, 0x2a, 0x2c, 0x0a, 0x00, 0x40, 0x42, 0x0f, 0x00, 0x80, 0x84, 0x0f,
  0x00, 0x80, 0x96, 0x98, 0x00, 0x32, 0x24, 0x05, 0x04, 0x00, 0xb0, 0x00, 0x90, 0x00, 0x00, 0xc6,
  0x00, 0x00, 0x00, 0x34, 0x17, 0x00, 0x00, 0xc6, 0x00, 0x00, 0x15, 0x16, 0x05, 0x00, 0x06, 0x15,
  0x16, 0x05, 0x00, 0x20, 0xa1, 0x07, 0x00, 0x2a, 0x2c, 0x0a, 0x00, 0x40, 0x42, 0x0f, 0x00, 0x80,
  0x84, 0x0f, 0x00, 0x80, 0x96, 0x98, 0x00, 0x32, 0x24, 0x05, 0x05, 0x00, 0x60, 0x01, 0x20, 0x01,
  0x00, 0x18, 0x03, 0x00, 0x00, 0xd0, 0x5c, 0x00, 0x00, 0x18, 0x03, 0x00, 0x15, 0x16, 0x05, 0x00,
  0x06, 0x15, 0x16, 0x05, 0x00, 0x20, 0xa1, 0x07, 0x00, 0x2a, 0x2c, 0x0a, 0x00, 0x40, 0x42, 0x0f,
  0x00, 0x80, 0x84, 0x0f, 0x00, 0x80, 0x96, 0x98, 0x00, 0x1a, 0x24, 0x03, 0x00, 0x05, 0x80, 0x02,
  0xe0, 0x01, 0x40, 0x01, 0xf0, 0x00, 0xa0, 0x00, 0x78, 0x00, 0xb0, 0x00, 0x90, 0x00, 0x60, 0x01,
  0x20, 0x01, 0x00, 0x06, 0x24, 0x0d, 0x01, 0x01, 0x04, 0x09, 0x04, 0x01, 0x01, 0x01, 0x0e, 0x02,
  0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0x60, 0x0a, 0x01, 0x09, 0x04, 0x01, 0x02, 0x01, 0x0e, 0x02,
  0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0x00, 0x0b, 0x01, 0x09, 0x04, 0x01, 0x03, 0x01, 0x0e, 0x02,
  0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0x20, 0x0b, 0x01, 0x09, 0x04, 0x01, 0x04, 0x01, 0x0e, 0x02,
  0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0x00, 0x13, 0x01, 0x09, 0x04, 0x01, 0x05, 0x01, 0x0e, 0x02,
  0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0x20, 0x13, 0x01, 0x09, 0x04, 0x01, 0x06, 0x01, 0x0e, 0x02,
  0x00, 0x00, 0x07, 0x05, 0x81, 0x05, 0xfc, 0x13, 0x01
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

MotionCamera::MotionCamera(EmulationKernel& ios) : m_ios(ios)
{
  m_id = (u64(m_vid) << 32 | u64(m_pid) << 16 | u64(9) << 8 | u64(1));
}

MotionCamera::~MotionCamera() {
  if (m_active_altsetting)
  {
    NOTICE_LOG_FMT(IOS_USB, "Host_CameraStop");
    Host_CameraStop();
  }
}

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
  //NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] Opening device", m_vid, m_pid);
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
    auto& system = m_ios.GetSystem();
    system.GetCameraBase().CreateSample(m_active_size.width, m_active_size.height);
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
                               CameraBase::getUVCRequest(cmd->request),
                               (unit == 0) ? CameraBase::getUVCVideoStreamingControl(control)
                               : (unit == 1) ? CameraBase::getUVCTerminalControl(control)
                               : (unit == 3) ? CameraBase::getUVCProcessingUnitControl(control)
                               : "");
      if (unit == 0 && control == VS_COMMIT)
      {
        auto& system = m_ios.GetSystem();
        auto& memory = system.GetMemory();
        UVCProbeCommitControl* commit = (UVCProbeCommitControl*) memory.GetPointerForRange(cmd->data_address, cmd->length);
        m_active_size = m_supported_sizes[commit->bFrameIndex - 1];
        m_delay = commit->dwFrameInterval / 10;
        u32 new_size = m_active_size.width * m_active_size.height * 2;
        if (m_image_size != new_size)
        {
          m_image_size = new_size;
          if (m_image_data)
          {
            free(m_image_data);
          }
          m_image_data = (u8*) calloc(1, m_image_size);
        }
        NOTICE_LOG_FMT(IOS_USB, "[{:04x}:{:04x}] VS_COMMIT: bFormatIndex={:02x} bFrameIndex={:02x} dwFrameInterval={:04x} / size={}x{} delay={}",
                                 m_vid, m_pid, commit->bFormatIndex, commit->bFrameIndex, commit->dwFrameInterval,
                                 m_active_size.width, m_active_size.height,
                                 m_delay
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
                               CameraBase::getUVCRequest(cmd->request),
                               (unit == 0) ? CameraBase::getUVCVideoStreamingControl(cmd->value >> 8)
                               : (unit == 1) ? CameraBase::getUVCTerminalControl(cmd->value >> 8)
                               : (unit == 3) ? CameraBase::getUVCProcessingUnitControl(cmd->value >> 8)
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

    u32 data_size = std::min(cmd->packet_sizes[i] - (u32)sizeof(uvc_header), m_image_size - m_image_pos);
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
    system.GetCameraBase().GetData(m_image_data, m_image_size);
    m_image_pos = 0;
  }

  // 15 fps, one frame every 66ms, half a frame per transfer, one transfer every 33ms
  cmd->ScheduleTransferCompletion(IPC_SUCCESS, m_delay);
  return IPC_SUCCESS;
}

void MotionCamera::ScheduleTransfer(std::unique_ptr<TransferCommand> command,
                                   const std::vector<u8>& data, u64 expected_time_us)
{
  command->FillBuffer(data.data(), static_cast<const size_t>(data.size()));
  command->ScheduleTransferCompletion(static_cast<s32>(data.size()), expected_time_us);
}
}  // namespace IOS::HLE::USB
