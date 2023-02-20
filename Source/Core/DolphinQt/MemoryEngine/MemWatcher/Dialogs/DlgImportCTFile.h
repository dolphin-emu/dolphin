#include <QButtonGroup>
#include <QDialog>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>

#include "MemoryEngine/Common/CommonTypes.h"

class DlgImportCTFile : public QDialog
{
public:
  DlgImportCTFile(QWidget* parent);

  void initialiseWidgets();
  void makeLayouts();
  QString getFileName() const;
  bool willUseDolphinPointers() const;
  u64 getCommonBase() const;
  void onAddressImportMethodChanged();
  void onBrowseFiles();
  void accept();

private:
  bool m_useDolphinPointers;
  u64 m_commonBase = 0;
  QString m_strFileName;
  QLineEdit* m_txbFileName;
  QPushButton* m_btnBrowseFiles;
  QLineEdit* m_txbCommonBase;
  QButtonGroup* m_btnGroupImportAddressMethod;
  QRadioButton* m_rdbUseDolphinPointers;
  QRadioButton* m_rdbUseCommonBase;
  QGroupBox* m_groupImportAddressMethod;
  QWidget* m_widgetAddressMethod;
};
