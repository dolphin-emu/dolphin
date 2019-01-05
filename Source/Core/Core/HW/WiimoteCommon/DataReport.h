// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteCommon/WiimoteConstants.h"
#include "Core/HW/WiimoteCommon/WiimoteHid.h"
#include "Core/HW/WiimoteCommon/WiimoteReport.h"

namespace WiimoteCommon
{
// Interface for manipulating Wiimote "Data" reports
// If a report does not contain a particular feature the Get/Set is a no-op.
class DataReportManipulator
{
public:
  virtual ~DataReportManipulator() = default;

  // Accel data handled as if there were always 10 bits of precision.
  struct AccelData
  {
    u16 x, y, z;
  };

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

  u8* data_ptr;
};

std::unique_ptr<DataReportManipulator> MakeDataReportManipulator(InputReportID rpt_id,
                                                                 u8* data_ptr);

class DataReportBuilder
{
public:
  explicit DataReportBuilder(InputReportID rpt_id);

  using CoreData = ButtonData;
  using AccelData = DataReportManipulator::AccelData;

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

private:
  static constexpr int HEADER_SIZE = 2;

  static constexpr int MAX_DATA_SIZE = MAX_PAYLOAD - 2;

  TypedHIDInputData<std::array<u8, MAX_DATA_SIZE>> m_data;

  std::unique_ptr<DataReportManipulator> m_manip;
};

}  // namespace WiimoteCommon
