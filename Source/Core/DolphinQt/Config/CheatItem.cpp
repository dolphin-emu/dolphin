// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/CheatItem.h"

#include "Core/ActionReplay.h"
#include "Core/GeckoCode.h"
#include "Core/PatchEngine.h"

static QString FormatName(std::string name) {
    return QString::fromStdString(name)
        .replace(QStringLiteral("&lt;"), QChar::fromLatin1('<'))
        .replace(QStringLiteral("&gt;"), QChar::fromLatin1('>'));
}

void CheatItem::Init(std::string name, bool enabled) {
    this->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable |
                Qt::ItemIsDragEnabled);
    this->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    this->setText(FormatName(name));
}

CheatItem::CheatItem(Gecko::GeckoCode code) : QListWidgetItem() {
    m_gecko_code = code;
    Init(code.name, code.enabled);
}

CheatItem::CheatItem(ActionReplay::ARCode code) : QListWidgetItem() {
    m_ar_code = code;
    Init(code.name, code.active);
}

CheatItem::CheatItem(PatchEngine::Patch patch) : QListWidgetItem(){
    m_patch = patch;
    Init(patch.name, patch.active);
}

template<>
Gecko::GeckoCode* CheatItem::GetRawCode() {
    m_gecko_code->enabled = this->checkState() == Qt::CheckState::Checked;
    return &(m_gecko_code.value());
}

template<>
ActionReplay::ARCode* CheatItem::GetRawCode() {
    m_ar_code->active = this->checkState() == Qt::CheckState::Checked;
    return &m_ar_code.value();
}

template<>
PatchEngine::Patch* CheatItem::GetRawCode() {
    m_patch->active = this->checkState() == Qt::CheckState::Checked;
    return &m_patch.value();
}

QString CheatItem::GetName() const {
    if (m_gecko_code)
        return QString::fromStdString(m_gecko_code->name);
    if (m_ar_code)
        return QString::fromStdString(m_ar_code->name);
    if (m_patch)
        return QString::fromStdString(m_patch->name);
    return QStringLiteral("<error>");
}

QString CheatItem::GetCreator() const {
    if (m_gecko_code) {
        // gecko codes have a creator
        QString::fromStdString(m_gecko_code->creator);
    }
    if (m_patch && !m_patch->user_defined) {
        // Some dolphin patches are shipped with dolphin
        return QStringLiteral("Dolphin Emulator");
    }
    // Otherwise, we don't know the creator of AR codes and user defined patches
    return QStringLiteral("");
}

QString CheatItem::GetNotes() const {
    QString notes;

    // Currently, only gecko codes have notes
    if (m_gecko_code) {
        for (const auto &line : m_gecko_code->notes) {
            notes += QString::fromStdString(line);
            notes += QStringLiteral("\n");
        }
    }

    return notes;
}

QString CheatItem::GetCode() const {
    QString code;

    if (m_gecko_code) {
        for (const auto& c : m_gecko_code->codes)
        {
            code += QStringLiteral("%1 %2")
                        .arg(c.address, 8, 16, QLatin1Char('0'))
                        .arg(c.data, 8, 16, QLatin1Char('0'));
        }
    }
    else if (m_ar_code) {
        for (const auto& e : m_ar_code->ops)
        {
            code += QStringLiteral("%1 %2\n")
                        .arg(e.cmd_addr, 8, 16, QLatin1Char('0'))
                        .arg(e.value, 8, 16, QLatin1Char('0'));
        }
    }

    return code;
}