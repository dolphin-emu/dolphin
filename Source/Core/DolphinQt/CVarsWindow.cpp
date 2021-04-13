#include "DolphinQt/CVarsWindow.h"

#include "Core/PrimeHack/Mods/ElfModLoader.h"
#include "Core/PrimeHack/PrimeUtils.h"

#include <QBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMenuBar>
#include <QFileDialog>
#include <QFontMetrics>
#include <QtGlobal>

CVarListModel::CVarListModel(QObject* parent) : QAbstractTableModel(parent) {
  load_cvars();
  update_memread();
}

void CVarListModel::get_column_widths(QFont const& font, std::array<int, CVarListModel::NUM_COLS>& width_out) {
  QFontMetrics fm(font);
  for (int i = 0; i < width_out.size(); i++) {
    int col_size = 0;
    for (int j = 0; j < cvar_list.size(); j++) {
      auto text = data(index(j, i));
#if QT_VERSION >= 0x050b00
      col_size = std::max(col_size, fm.horizontalAdvance(text.toString()));
#else
      col_size = std::max(col_size, fm.width(text.toString()));
#endif
    }
    width_out[i] = col_size;
  }
}

void CVarListModel::update_memread() {
  cached_vals.clear();
  const auto get_value = [](prime::CVar const& var) -> QVariant {
    switch (var.type) {
    case prime::CVarType::INT8:
      return prime::read8(var.addr);
    case prime::CVarType::INT16:
      return prime::read16(var.addr);
    case prime::CVarType::INT32:
      return prime::read32(var.addr);
    case prime::CVarType::INT64:
      return qulonglong{prime::read64(var.addr)};
    case prime::CVarType::FLOAT32:
      return prime::readf32(var.addr);
    case prime::CVarType::FLOAT64: {
      u64 tmp = prime::read64(var.addr);
      return *reinterpret_cast<double*>(&tmp);
    }
    case prime::CVarType::BOOLEAN:
      return static_cast<bool>(prime::read8(var.addr));
    default:
      return 0u;
    }
  };

  for (auto const& var : cvar_list) {
    cached_vals.push_back(get_value(var));
  }
}

QVariant CVarListModel::data(const QModelIndex& index, int role) const {
  if (!index.isValid()) {
    return QVariant();
  }
  if (role != Qt::DisplayRole) {
    return QVariant();
  }

  const auto get_typename = [](prime::CVar const& var) -> QVariant {
    switch (var.type) {
    case prime::CVarType::INT8:
      return QString::fromStdString("int8");
    case prime::CVarType::INT16:
      return QString::fromStdString("int16");
    case prime::CVarType::INT32:
      return QString::fromStdString("int32");
    case prime::CVarType::INT64:
      return QString::fromStdString("int64");
    case prime::CVarType::FLOAT32:
      return QString::fromStdString("float32");
    case prime::CVarType::FLOAT64:
      return QString::fromStdString("float64");
    case prime::CVarType::BOOLEAN:
      return QString::fromStdString("boolean");
    default:
      return QString::fromStdString("invalid");
    }
  };

  switch (index.column()) {
  case COL_NAME:
    return QString::fromStdString(cvar_list[index.row()].name);
  case COL_VALUE:
    if (index.row() < cached_vals.size()) {
      return cached_vals[index.row()];
    } else {
      return QVariant();
    }
  case COL_TYPE:
    return get_typename(cvar_list[index.row()]);
  default:
    return QVariant();
  }
}

QVariant CVarListModel::headerData(int section, Qt::Orientation orientation, int role) const {
  if (orientation == Qt::Vertical || role != Qt::DisplayRole) {
    return QVariant();
  }

  switch (section) {
  case COL_NAME:
    return tr("Name");
  case COL_VALUE:
    return tr("Value");
  case COL_TYPE:
    return tr("Type");
  default:
    return QVariant();
  }
}

int CVarListModel::rowCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return static_cast<int>(cvar_list.size());
}

int CVarListModel::columnCount(const QModelIndex& parent) const {
  if (parent.isValid()) {
    return 0;
  }
  return NUM_COLS;
}

void CVarListModel::load_cvars() {
  prime::ElfModLoader* mod = static_cast<prime::ElfModLoader*>(prime::GetHackManager()->get_mod("elf_mod_loader"));
  if (mod == nullptr) {
    return;
  }
  mod->get_cvarlist(cvar_list);
}

CVarsWindow::CVarsWindow(QWidget* parent) : QDialog(parent), list_model(this) {
  setWindowTitle(tr("CVars"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  cvar_list = new QTableView(this);
  cvar_list->setModel(&list_model);
  cvar_list->setTabKeyNavigation(false);
  cvar_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  cvar_list->setSelectionBehavior(QAbstractItemView::SelectRows);
  cvar_list->setShowGrid(false);
  cvar_list->setSortingEnabled(false);
  cvar_list->setCurrentIndex(QModelIndex());
  cvar_list->setContextMenuPolicy(Qt::CustomContextMenu);
  cvar_list->setWordWrap(false);
  
  cvar_list->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

  std::array<int, 3> cols;
  list_model.get_column_widths(cvar_list->font(), cols);
  int min_width = 15 + style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  for (int i = 0; i < cols.size(); i++) {
    cvar_list->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Fixed);
    cvar_list->horizontalHeader()->resizeSection(i, cols[i] + 10);
    min_width += cols[i] + 10;
  }

  cvar_list->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
  cvar_list->setMinimumSize(QSize(min_width, 300));

  QMenuBar* menu_bar = new QMenuBar(this);
  auto* file_menu = menu_bar->addMenu(tr("File"));
  file_menu->addAction(tr("Load CVar Presets"), this, &CVarsWindow::load_presets);
  file_menu->addAction(tr("Save CVar Presets"), this, &CVarsWindow::save_presets);

  QVBoxLayout* vbox_layout = new QVBoxLayout(this);
  vbox_layout->addWidget(cvar_list);
  vbox_layout->setMenuBar(menu_bar);

  setLayout(vbox_layout);
  layout()->setSizeConstraint(QLayout::SetFixedSize);

  connect(cvar_list, &QTableView::doubleClicked, this, &CVarsWindow::CVarClicked);
}

void CVarsWindow::save_presets() {
  QString path = QFileDialog::getSaveFileName(this, tr("Save CVar Configuration to File"), QString(),
                                              tr("CFG file (*.cfg);; All Files (*)"));

  std::ofstream file_out(path.toStdString(), std::ios::trunc);
  if (!file_out.is_open()) {
    return;
  }

  for (auto const& var : list_model.get_var_list()) {
    std::ostringstream line;
    line << var.name << "=";
    switch (var.type) {
    case prime::CVarType::BOOLEAN:
      line << (prime::read8(var.addr) ? "true" : "false");
      break;
    case prime::CVarType::INT8:
      line << prime::read8(var.addr);
      break;
    case prime::CVarType::INT16:
      line << prime::read16(var.addr);
      break;
    case prime::CVarType::INT32:
      line << prime::read32(var.addr);
      break;
    case prime::CVarType::INT64:
      line << prime::read64(var.addr);
      break;
    case prime::CVarType::FLOAT32:
      line << prime::readf32(var.addr);
      break;
    case prime::CVarType::FLOAT64: {
        u64 tmp = prime::read64(var.addr);
        line << *reinterpret_cast<double*>(&tmp);
      }
      break;
    }
    file_out << line.str() << "\n";
  }
  file_out.close();
}

void CVarsWindow::load_presets() {
  QString path = QFileDialog::getOpenFileName(this, tr("Select CFG file to load"), QString(),
                                                tr("CFG file (*.cfg);; All Files (*)"));
  prime::ElfModLoader* mod = static_cast<prime::ElfModLoader*>(prime::GetHackManager()->get_mod("elf_mod_loader"));
  if (mod == nullptr) {
    return;
  }
  mod->load_presets(path.toStdString());
  
  list_model.update_memread();
  emit list_model.dataChanged(list_model.index(0, 0), list_model.index(list_model.cvar_count(), 3));
}

CVarEditDialog::CVarEditDialog(prime::CVar* in_var, QWidget* parent) : QDialog(parent), var(in_var) {
  setWindowTitle(tr("Set CVar..."));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  QVBoxLayout* layout = new QVBoxLayout(this);
  QLabel* cvar_label = new QLabel(this);
  cvar_label->setText(QString(tr("Set value for CVar %1")).arg(tr(in_var->name.c_str())));
  text_entry = new QLineEdit(this);
  text_entry->setMinimumWidth(200);
  QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  layout->addWidget(cvar_label);
  layout->addWidget(text_entry);
  layout->addWidget(buttons);

  connect(buttons, &QDialogButtonBox::accepted, this, &CVarEditDialog::OnEnter);
  connect(buttons, &QDialogButtonBox::rejected, this, &CVarEditDialog::close);
}

void CVarsWindow::on_value_entry(prime::CVar* var, QString const& new_val) {
  prime::ElfModLoader* mod = static_cast<prime::ElfModLoader*>(prime::GetHackManager()->get_mod("elf_mod_loader"));
  if (mod == nullptr) {
    return;
  }

  const auto cmp_no_case = [] (std::string const& str, std::string_view rhs) -> bool {
    return std::equal(str.begin(), str.end(), rhs.begin(), rhs.end(),
      [](char l, char r) -> bool { return tolower(l) == tolower(r); });
  };

  switch (var->type) {
  case prime::CVarType::INT8: {
      u8 val = static_cast<u8>(new_val.toInt());
      mod->write_cvar(var->name, &val);
    }
    break;
  case prime::CVarType::INT16: {
      u16 val = static_cast<u16>(new_val.toInt());
      mod->write_cvar(var->name, &val);
    }
    break;
  case prime::CVarType::INT32: {
      u32 val = static_cast<u32>(new_val.toInt());
      mod->write_cvar(var->name, &val);
    }
    break;
  case prime::CVarType::INT64: {
      u64 val = static_cast<u64>(new_val.toLongLong());
      mod->write_cvar(var->name, &val);
    }
    break;
  case prime::CVarType::FLOAT32: {
      float val = new_val.toFloat();
      mod->write_cvar(var->name, &val);
    }
    break;
  case prime::CVarType::FLOAT64: {
      double val = new_val.toDouble();
      mod->write_cvar(var->name, &val);
    }
    break;
  case prime::CVarType::BOOLEAN: {
      std::string val_str = new_val.toStdString();
      bool val;
      if (cmp_no_case(val_str, "true") ||
          cmp_no_case(val_str, "on") ||
          cmp_no_case(val_str, "yes") ||
          cmp_no_case(val_str, "ok")) {
        val = true;
        mod->write_cvar(var->name, &val);
      } else if (cmp_no_case(val_str, "false") ||
                 cmp_no_case(val_str, "off") ||
                 cmp_no_case(val_str, "no")) {
        val = false;
        mod->write_cvar(var->name, &val);
      }
    }
    break;
  }
  list_model.update_memread();
  emit list_model.dataChanged(list_model.index(0, 0), list_model.index(list_model.cvar_count(), 3));
}

void CVarsWindow::CVarClicked(QModelIndex const& idx) {
  if (!idx.isValid()) {
    return;
  }
  CVarEditDialog* edit_dlg = new CVarEditDialog(list_model.get_var(idx.row()), this);
  connect(edit_dlg, &CVarEditDialog::OnEnter, this, [this, edit_dlg] () {
    on_value_entry(edit_dlg->get_var(), edit_dlg->get_entry_val());
    edit_dlg->close();
  });
  edit_dlg->exec();
  delete edit_dlg;
}
