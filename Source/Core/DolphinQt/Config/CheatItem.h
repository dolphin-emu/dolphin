// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QListWidget>
#include <QObject>
#include <optional>

#include "Core/ActionReplay.h"
#include "Core/GeckoCode.h"
#include "Core/PatchEngine.h"

class CheatItem : public QListWidgetItem {
public:
    CheatItem(Gecko::GeckoCode code);
    CheatItem(ActionReplay::ARCode code);
    CheatItem(PatchEngine::Patch patch);

    QString GetName() const;
    QString GetCreator() const;
    QString GetNotes() const;
    QString GetCode() const;

    template<typename T>
    T* GetRawCode();
private:
    void Init(std::string, bool);

    std::optional<Gecko::GeckoCode> m_gecko_code;
    std::optional<ActionReplay::ARCode> m_ar_code;
    std::optional<PatchEngine::Patch> m_patch;
};