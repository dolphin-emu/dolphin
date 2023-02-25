// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QSortFilterProxyModel>

// This subclass of QSortFilterProxyModel allows the data to be filtered by the view.
class ListProxyModel final : public QSortFilterProxyModel
{
  Q_OBJECT

public:
  explicit ListProxyModel(QObject* parent = nullptr);
  bool filterAcceptsRow(int source_row, const QModelIndex& source_parent) const override;

protected:
  bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;
};
