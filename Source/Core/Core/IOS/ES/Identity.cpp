// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/IOS/ES/ES.h"

#include <vector>

#include <mbedtls/sha1.h>

#include "Common/Crypto/ec.h"
#include "Common/Logging/Log.h"
#include "Common/ScopeGuard.h"
#include "Common/StringUtil.h"
#include "Core/ConfigManager.h"
#include "Core/HW/Memmap.h"
#include "Core/IOS/ES/Formats.h"
#include "Core/IOS/IOSC.h"
#include "Core/IOS/Uids.h"

namespace IOS::HLE::Device
{
ReturnCode ES::GetDeviceId(u32* device_id) const
{
  *device_id = m_ios.GetIOSC().GetDeviceId();
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

  const IOS::CertECC cert = m_ios.GetIOSC().GetDeviceCertificate();
  Memory::CopyToEmu(request.io_vectors[0].address, &cert, sizeof(cert));
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

  if (!m_title_context.active)
    return GetDefaultReply(ES_EINVAL);

  m_ios.GetIOSC().Sign(sig_out, ap_cert_out, m_title_context.tmd.GetTitleId(), data, data_size);
  return GetDefaultReply(IPC_SUCCESS);
}

ReturnCode ES::VerifySign(const std::vector<u8>& hash, const std::vector<u8>& ecc_signature,
                          const std::vector<u8>& certs_bytes)
{
  if (!SConfig::GetInstance().m_enable_signature_checks)
  {
    WARN_LOG(IOS_ES, "VerifySign: signature checks are disabled. Skipping.");
    return IPC_SUCCESS;
  }

  const std::map<std::string, IOS::ES::CertReader> certs = IOS::ES::ParseCertChain(certs_bytes);
  if (certs.empty())
    return ES_EINVAL;

  const auto ap_iterator = std::find_if(certs.begin(), certs.end(), [](const auto& entry) {
    return entry.first.length() > 2 && entry.first.compare(0, 2, "AP") == 0;
  });
  if (ap_iterator == certs.end())
    return ES_UNKNOWN_ISSUER;
  const IOS::ES::CertReader& ap = ap_iterator->second;

  const auto ap_issuers = SplitString(ap.GetIssuer(), '-');
  const auto ng_iterator = ap_issuers.size() > 1 ? certs.find(*ap_issuers.rbegin()) : certs.end();
  if (ng_iterator == certs.end())
    return ES_UNKNOWN_ISSUER;
  const IOS::ES::CertReader& ng = ng_iterator->second;

  IOSC& iosc = m_ios.GetIOSC();
  IOSC::Handle ng_cert;
  ReturnCode ret = iosc.CreateObject(&ng_cert, IOSC::TYPE_PUBLIC_KEY, IOSC::SUBTYPE_ECC233, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;
  Common::ScopeGuard handle_guard{[&] { iosc.DeleteObject(ng_cert, PID_ES); }};

  ret = VerifyContainer(VerifyContainerType::Device, VerifyMode::DoNotUpdateCertStore, ng,
                        certs_bytes, ng_cert);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG(IOS_ES, "VerifySign: VerifyContainer(ng) failed with error %d", ret);
    return ret;
  }

  ret = iosc.VerifyPublicKeySign(ap.GetSha1(), ng_cert, ap.GetSignatureData(), PID_ES);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG(IOS_ES, "VerifySign: IOSC_VerifyPublicKeySign(ap) failed with error %d", ret);
    return ret;
  }

  IOSC::Handle ap_cert;
  ret = iosc.CreateObject(&ap_cert, IOSC::TYPE_PUBLIC_KEY, IOSC::SUBTYPE_ECC233, PID_ES);
  if (ret != IPC_SUCCESS)
    return ret;
  Common::ScopeGuard handle2_guard{[&] { iosc.DeleteObject(ap_cert, PID_ES); }};

  ret = iosc.ImportPublicKey(ap_cert, ap.GetPublicKey().data(), nullptr, PID_ES);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG(IOS_ES, "VerifySign: IOSC_ImportPublicKey(ap) failed with error %d", ret);
    return ret;
  }

  std::array<u8, 20> sha1;
  mbedtls_sha1_ret(hash.data(), hash.size(), sha1.data());
  ret = iosc.VerifyPublicKeySign(sha1, ap_cert, ecc_signature, PID_ES);
  if (ret != IPC_SUCCESS)
  {
    ERROR_LOG(IOS_ES, "VerifySign: IOSC_VerifyPublicKeySign(data) failed with error %d", ret);
    return ret;
  }

  INFO_LOG(IOS_ES, "VerifySign: all checks passed");
  return IPC_SUCCESS;
}

IPCCommandResult ES::VerifySign(const IOCtlVRequest& request)
{
  if (!request.HasNumberOfValidVectors(3, 0))
    return GetDefaultReply(ES_EINVAL);
  if (request.in_vectors[1].size != sizeof(Common::ec::Signature))
    return GetDefaultReply(ES_EINVAL);

  std::vector<u8> hash(request.in_vectors[0].size);
  Memory::CopyFromEmu(hash.data(), request.in_vectors[0].address, hash.size());

  std::vector<u8> ecc_signature(request.in_vectors[1].size);
  Memory::CopyFromEmu(ecc_signature.data(), request.in_vectors[1].address, ecc_signature.size());

  std::vector<u8> certs(request.in_vectors[2].size);
  Memory::CopyFromEmu(certs.data(), request.in_vectors[2].address, certs.size());

  return GetDefaultReply(VerifySign(hash, ecc_signature, certs));
}
}  // namespace IOS::HLE::Device
