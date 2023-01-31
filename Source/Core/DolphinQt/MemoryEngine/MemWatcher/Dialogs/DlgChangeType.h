#pragma once

#include <QComboBox>
#include <QDialog>
#include <QSpinBox>

class DlgChangeType : public QDialog
{
  Q_OBJECT

public:
  DlgChangeType(QWidget* parent, const int typeIndex, const size_t length);
  int getTypeIndex() const;
  size_t getLength() const;
  void accept();
  void onTypeChange(int index);

private:
  void initialiseWidgets();
  void makeLayouts();

  int m_typeIndex;
  size_t m_length;
  QComboBox* m_cmbTypes;
  QSpinBox* m_spnLength;
  QWidget* m_lengthSelection;
};
