// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"
#include "Common/Matrix.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

namespace WiimoteCommon
{
// Interface for manipulating Wiimote "Data" reports
// If a report does not contain a particular feature the Get/Set is a no-op.
class DataReportManipulator
{
public:
  virtual ~DataReportManipulator() = default;

  using CoreData = ButtonData;

  virtual bool HasCore() const = 0;
  virtual bool HasAccel() const = 0;
  bool HasIR() const;
  bool HasExt() const;

  virtual void GetCoreData(CoreData*) const = 0;
  virtual void GetAccelData(AccelData*) const = 0;

  virtual void SetCoreData(const CoreData&) = 0;
  virtual void SetAccelData(const AccelData&) = 0;

  virtual u8* GetIRDataPtr() = 0;
  virtual const u8* GetIRDataPtr() const = 0;
  virtual u32 GetIRDataSize() const = 0;
  virtual u32 GetIRDataFormatOffset() const = 0;

  virtual u8* GetExtDataPtr() = 0;
  virtual const u8* GetExtDataPtr() const = 0;
  virtual u32 GetExtDataSize() const = 0;

  u8* GetDataPtr();
  const u8* GetDataPtr() const;

  virtual u32 GetDataSize() const = 0;

  u8* data_ptr = nullptr;
};

std::unique_ptr<DataReportManipulator> MakeDataReportManipulator(InputReportID rpt_id,
                                                                 u8* data_ptr);

class DataReportBuilder
{
public:
  explicit DataReportBuilder(InputReportID rpt_id);

  using CoreData = ButtonData;

  void SetMode(InputReportID rpt_id);
  InputReportID GetMode() const;

  static bool IsValidMode(InputReportID rpt_id);

  bool HasCore() const;
  bool HasAccel() const;
  bool HasIR() const;
  bool HasExt() const;

  u32 GetIRDataSize() const;
  u32 GetExtDataSize() const;

  u32 GetIRDataFormatOffset() const;

  void GetCoreData(CoreData*) const;
  void GetAccelData(AccelData*) const;

  void SetCoreData(const CoreData&);
  void SetAccelData(const AccelData&);

  u8* GetIRDataPtr();
  const u8* GetIRDataPtr() const;
  u8* GetExtDataPtr();
  const u8* GetExtDataPtr() const;

  u8* GetDataPtr();
  const u8* GetDataPtr() const;

  u32 GetDataSize() const;

  // The largest report is 0x3d (21 extension bytes).
  static constexpr int MAX_DATA_SIZE = 21;

private:
  TypedInputData<std::array<u8, MAX_DATA_SIZE>> m_data;

  std::unique_ptr<DataReportManipulator> m_manip;
};

}  // namespace WiimoteCommon
