// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>

#include <QSortFilterProxyModel>
#include <QString>

#include "Common/CommonTypes.h"

namespace Core
{
class BranchWatch;
}
class BranchWatchDialog;
class BranchWatchTableModel;
class QGridLayout;

class BranchWatchProxyModel final : public QSortFilterProxyModel
{
  Q_OBJECT

  friend BranchWatchDialog;

public:
  explicit BranchWatchProxyModel(const Core::BranchWatch& branch_watch, QObject* parent = nullptr)
      : QSortFilterProxyModel(parent), m_branch_watch(branch_watch)
  {
  }
  BranchWatchTableModel* sourceModel() const;
  bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

  template <bool BranchWatchProxyModel::*member>
  void OnToggled(bool enabled)
  {
    this->*member = enabled;
    invalidateRowsFilter();
  }
  void OnSymbolTextChanged(const QString& text)
  {
    m_symbol_name = text;
    invalidateRowsFilter();
  }
  template <std::optional<u32> BranchWatchProxyModel::*member>
  void OnAddressTextChanged(const QString& text)
  {
    bool ok;
    if (const u32 value = text.toUInt(&ok, 16); ok)
      this->*member = value;
    else
      this->*member = std::nullopt;
    invalidateRowsFilter();
  }
  void OnDelete(const QModelIndex& index);
  void OnDelete(QModelIndexList index_list);

  bool IsBranchFiltered(u32 hex) const;

private:
  const Core::BranchWatch& m_branch_watch;

  QString m_symbol_name = {};
  std::optional<u32> m_origin_min, m_origin_max, m_destin_min, m_destin_max;
  bool m_b = {}, m_bl = {}, m_bc = {}, m_bcl = {}, m_blr = {}, m_blrl = {}, m_bclr = {},
       m_bclrl = {}, m_bctr = {}, m_bctrl = {}, m_bcctr = {}, m_bcctrl = {};
};
