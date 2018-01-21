// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <cstring>
#include <vector>

#include "Common/Logging/Log.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/ec_wii.h"

namespace IOS
{
namespace HLE
{
namespace Device
{
ReturnCode ES::GetDeviceId(u32* device_id) const
{
  const EcWii& ec = EcWii::GetInstance();
  *device_id = ec.GetNGID();
  INFO_LOG(IOS_ES, "GetDeviceId: %08X", *device_id);
  return IPC_SUCCESS;
}

IPCCommandResult ES::GetDeviceId(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1) || request.io_vectors[0].size != sizeof(u32))
    return GetDefaultReply(ES_EINVAL);

  u32 device_id;
  const ReturnCode ret = GetDeviceId(&device_id);
  if (ret != IPC_SUCCESS)
    return GetDefaultReply(ret);
  Memory::Write_U32(device_id, request.io_vectors[0].address);
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Encrypt(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 2))
    return GetDefaultReply(ES_EINVAL);

  u32 keyIndex = Memory::Read_U32(request.in_vectors[0].address);
  u8* source = Memory::GetPointer(request.in_vectors[2].address);
  u32 size = request.in_vectors[2].size;
  u8* iv = Memory::GetPointer(request.io_vectors[0].address);
  u8* destination = Memory::GetPointer(request.io_vectors[1].address);

  // TODO: Check whether the active title is allowed to encrypt.

  const ReturnCode ret = m_ios.GetIOSC().Encrypt(keyIndex, iv, source, size, destination, PID_ES);
  return GetDefaultReply(ret);
}

IPCCommandResult ES::Decrypt(u32 uid, const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 2))
    return GetDefaultReply(ES_EINVAL);

  u32 keyIndex = Memory::Read_U32(request.in_vectors[0].address);
  u8* source = Memory::GetPointer(request.in_vectors[2].address);
  u32 size = request.in_vectors[2].size;
  u8* iv = Memory::GetPointer(request.io_vectors[0].address);
  u8* destination = Memory::GetPointer(request.io_vectors[1].address);

  // TODO: Check whether the active title is allowed to decrypt.

  const ReturnCode ret = m_ios.GetIOSC().Decrypt(keyIndex, iv, source, size, destination, PID_ES);
  return GetDefaultReply(ret);
}

IPCCommandResult ES::CheckKoreaRegion(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 0))
    return GetDefaultReply(ES_EINVAL);

  // note by DacoTaco : name is unknown, I just tried to name it SOMETHING.
  // IOS70 has this to let system menu 4.2 check if the console is region changed. it returns
  // -1017
  // if the IOS didn't find the Korean keys and 0 if it does. 0 leads to a error 003
  INFO_LOG(IOS_ES, "IOCTL_ES_CHECKKOREAREGION: Title checked for Korean keys.");
  return GetDefaultReply(ES_EINVAL);
}

IPCCommandResult ES::GetDeviceCertificate(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(0, 1) || request.io_vectors[0].size != 0x180)
    return GetDefaultReply(ES_EINVAL);

  INFO_LOG(IOS_ES, "IOCTL_ES_GETDEVICECERT");
  u8* destination = Memory::GetPointer(request.io_vectors[0].address);

  const EcWii& ec = EcWii::GetInstance();
  MakeNGCert(destination, ec.GetNGID(), ec.GetNGKeyID(), ec.GetNGPriv(), ec.GetNGSig());
  return GetDefaultReply(IPC_SUCCESS);
}

IPCCommandResult ES::Sign(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(1, 2))
    return GetDefaultReply(ES_EINVAL);

  INFO_LOG(IOS_ES, "IOCTL_ES_SIGN");
  u8* ap_cert_out = Memory::GetPointer(request.io_vectors[1].address);
  u8* data = Memory::GetPointer(request.in_vectors[0].address);
  u32 data_size = request.in_vectors[0].size;
  u8* sig_out = Memory::GetPointer(request.io_vectors[0].address);

  if (!GetTitleContext().active)
    return GetDefaultReply(ES_EINVAL);

  const EcWii& ec = EcWii::GetInstance();
  MakeAPSigAndCert(sig_out, ap_cert_out, GetTitleContext().tmd.GetTitleId(), data, data_size,
                   ec.GetNGPriv(), ec.GetNGID());

  return GetDefaultReply(IPC_SUCCESS);
}
}  // namespace Device
}  // namespace HLE
}  // namespace IOS
