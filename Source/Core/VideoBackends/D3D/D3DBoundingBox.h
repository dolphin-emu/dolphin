// Copyright 2014 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "VideoBackends/D3D/D3DBase.h"

#include "VideoCommon/BoundingBox.h"

namespace DX11
{
class D3DBoundingBox final : public BoundingBox
{
public:
  ~D3DBoundingBox() override;

  bool Initialize() override;

protected:
  std::vector<BBoxType> Read(u32 index, u32 length) override;
  void Write(u32 index, const std::vector<BBoxType>& values) override;

private:
  ComPtr<ID3D11Buffer> m_buffer;
  ComPtr<ID3D11Buffer> m_staging_buffer;
  ComPtr<ID3D11UnorderedAccessView> m_uav;
};

}  // namespace DX11
