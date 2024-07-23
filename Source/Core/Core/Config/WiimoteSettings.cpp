// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Core/Config/WiimoteSettings.h"

#include "Core/HW/Wiimote.h"

namespace Config
{
const Info WIIMOTE_1_SOURCE{{System::WiiPad, "Wiimote1", "Source"},
                                           WiimoteSource::Emulated};
const Info WIIMOTE_2_SOURCE{{System::WiiPad, "Wiimote2", "Source"},
                                           WiimoteSource::None};
const Info WIIMOTE_3_SOURCE{{System::WiiPad, "Wiimote3", "Source"},
                                           WiimoteSource::None};
const Info WIIMOTE_4_SOURCE{{System::WiiPad, "Wiimote4", "Source"},
                                           WiimoteSource::None};
const Info WIIMOTE_BB_SOURCE{{System::WiiPad, "BalanceBoard", "Source"},
                                            WiimoteSource::None};

const Info<WiimoteSource>& GetInfoForWiimoteSource(const int index)
{
  static constexpr std::array infos{
      &WIIMOTE_1_SOURCE, &WIIMOTE_2_SOURCE,  &WIIMOTE_3_SOURCE,
      &WIIMOTE_4_SOURCE, &WIIMOTE_BB_SOURCE,
  };
  return *infos[index];
}

}  // namespace Config
