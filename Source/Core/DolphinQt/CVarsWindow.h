#pragma once

#pragma once

#include <array>
#include <vector>

#include <QAbstractTableModel>
#include <QDialog>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QLineEdit>

#include "Core/PrimeHack/Mods/ElfModLoader.h"

class CVarListModel final : public QAbstractTableModel {
  Q_OBJECT
public:
  enum CVarColType {
    COL_NAME = 0,
    COL_VALUE,
    COL_TYPE,
    NUM_COLS
  };

  explicit CVarListModel(QObject* parent = nullptr);

  QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role = Qt::DisplayRole) const override;
  int rowCount(const QModelIndex& parent) const override;
  int columnCount(const QModelIndex& parent) const override;
  void load_cvars();

  int cvar_count() { return static_cast<int>(cvar_list.size()); }
  void get_column_widths(QFont const& font, std::array<int, NUM_COLS>& width_out);
  prime::CVar* get_var(int row) {
    return &cvar_list[row];
  }
  std::vector<prime::CVar> const& get_var_list() { return cvar_list; } 
  void update_memread();

private:
  std::vector<prime::CVar> cvar_list;
  std::vector<QVariant> cached_vals;
};

class CVarEditDialog : public QDialog {
  Q_OBJECT
public:
  explicit CVarEditDialog(prime::CVar* in_var, QWidget* parent = nullptr);

  prime::CVar* get_var() { return var; }
  QString get_entry_val() { return text_entry->text(); }

signals:
  void OnEnter();

private:
  prime::CVar* var;
  QLineEdit* text_entry;
};

class CVarsWindow final : public QDialog {
  Q_OBJECT
public:
  explicit CVarsWindow(QWidget* parent = nullptr);

  void load_presets();
  void save_presets();
  void on_value_entry(prime::CVar* var, QString const& val);
private:
  QTableView* cvar_list;
  QSortFilterProxyModel* cvar_list_proxy;
  CVarListModel list_model;

  void CVarClicked(QModelIndex const& idx);
};
