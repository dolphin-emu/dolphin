#pragma once

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVector>

#include "MemoryEngine/MemoryWatch/MemWatchEntry.h"

class DlgAddWatchEntry : public QDialog
{
public:
  DlgAddWatchEntry(MemWatchEntry* entry);
  ~DlgAddWatchEntry();
  void onTypeChange(int index);
  void accept();
  void onAddressChanged();
  void onIsPointerChanged();
  void onLengthChanged();
  void onOffsetChanged();
  MemWatchEntry* getEntry() const;

private:
  void initialiseWidgets();
  void makeLayouts();
  void fillFields(MemWatchEntry* entry);

  void updatePreview();
  bool validateAndSetAddress();
  bool validateAndSetOffset(int index);
  void addPointerOffset();
  void removePointerOffset();
  void removeAllPointerOffset();

  MemWatchEntry* m_entry;
  QLineEdit* m_txbAddress;
  QVector<QLineEdit*> m_offsets;
  QVector<QLabel*> m_addressPath;
  QGridLayout* m_offsetsLayout;
  QCheckBox* m_chkBoundToPointer;
  QLabel* m_lblValuePreview;
  QLineEdit* m_txbLabel;
  QComboBox* m_cmbTypes;
  QSpinBox* m_spnLength;
  QWidget* m_lengtWidget;
  QWidget* m_pointerWidget;
  QPushButton* m_btnAddOffset;
  QPushButton* m_btnRemoveOffset;
};
