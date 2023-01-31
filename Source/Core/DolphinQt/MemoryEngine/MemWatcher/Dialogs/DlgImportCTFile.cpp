#include "DlgImportCTFile.h"

#include <sstream>

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QVBoxLayout>

DlgImportCTFile::DlgImportCTFile(QWidget* parent)
{
  initialiseWidgets();
  makeLayouts();
}

void DlgImportCTFile::initialiseWidgets()
{
  m_useDolphinPointers = false;
  m_strFileName = QString();

  m_txbFileName = new QLineEdit(tr(""));
  m_txbFileName->setReadOnly(true);

  m_txbCommonBase = new QLineEdit(tr(""));
  m_txbCommonBase->setPlaceholderText(tr("Hex number ex. 7FFF0000"));

  m_widgetAddressMethod = new QWidget();

  m_btnBrowseFiles = new QPushButton(tr("Browse..."));
  connect(m_btnBrowseFiles, &QPushButton::clicked, this, &DlgImportCTFile::onBrowseFiles);

  m_groupImportAddressMethod =
      new QGroupBox(tr("Addresses import method (select the option that describes the table)"));

  m_rdbUseCommonBase = new QRadioButton(tr("Using an assumed common RAM start address"));
  m_rdbUseDolphinPointers = new QRadioButton(tr("Using an internal Dolphin pointer"));

  m_btnGroupImportAddressMethod = new QButtonGroup();
  m_btnGroupImportAddressMethod->addButton(m_rdbUseCommonBase, 0);
  m_btnGroupImportAddressMethod->addButton(m_rdbUseDolphinPointers, 1);
  m_rdbUseCommonBase->setChecked(true);

  connect(m_btnGroupImportAddressMethod, QOverload<int>::of(&QButtonGroup::buttonClicked),
          [=](int id) { onAddressImportMethodChanged(); });
}

void DlgImportCTFile::makeLayouts()
{
  QLabel* lblFileSelect = new QLabel(tr("Select the Cheat Table file to import: "));

  QHBoxLayout* fileSelectInput_layout = new QHBoxLayout();
  fileSelectInput_layout->addWidget(m_txbFileName);
  fileSelectInput_layout->addWidget(m_btnBrowseFiles);

  QVBoxLayout* fileSelect_layout = new QVBoxLayout();
  fileSelect_layout->setContentsMargins(0, 0, 0, 10);
  fileSelect_layout->addWidget(lblFileSelect);
  fileSelect_layout->addLayout(fileSelectInput_layout);

  QLabel* lblCommonBaseInput = new QLabel(tr("Enter the assumed start address: "));

  QHBoxLayout* commonBase_layout = new QHBoxLayout();
  commonBase_layout->setContentsMargins(20, 0, 0, 0);
  commonBase_layout->addWidget(lblCommonBaseInput);
  commonBase_layout->addWidget(m_txbCommonBase);

  m_widgetAddressMethod->setLayout(commonBase_layout);

  QLabel* lblCommonBase =
      new QLabel(tr("The table assumes a common RAM start address, every entry\n"
                    "use an address from the address field of the entry and are\n"
                    "offset according to the assumed RAM start address"));
  lblCommonBase->setContentsMargins(20, 0, 0, 0);
  QLabel* lblDolphinPointer =
      new QLabel(tr("Every entry of the table use a Dolphin internal pointer\n"
                    "that points to the start of the RAM and the offset from\n"
                    "the start is in the offset field of the entry"));
  lblDolphinPointer->setContentsMargins(20, 0, 0, 0);

  QVBoxLayout* addressImport_layout = new QVBoxLayout();
  addressImport_layout->addWidget(m_rdbUseCommonBase);
  addressImport_layout->addWidget(lblCommonBase);
  addressImport_layout->addWidget(m_widgetAddressMethod);
  addressImport_layout->addWidget(m_rdbUseDolphinPointers);
  addressImport_layout->addWidget(lblDolphinPointer);

  m_groupImportAddressMethod->setLayout(addressImport_layout);

  QDialogButtonBox* buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);

  QVBoxLayout* main_layout = new QVBoxLayout;
  main_layout->addLayout(fileSelect_layout);
  main_layout->addWidget(m_groupImportAddressMethod);
  main_layout->addWidget(buttonBox);
  main_layout->addStretch();
  setLayout(main_layout);

  connect(buttonBox, &QDialogButtonBox::accepted, this, &DlgImportCTFile::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, this, &DlgImportCTFile::reject);
}

QString DlgImportCTFile::getFileName() const
{
  return m_strFileName;
}

bool DlgImportCTFile::willUseDolphinPointers() const
{
  return m_useDolphinPointers;
}

u64 DlgImportCTFile::getCommonBase() const
{
  return m_commonBase;
}

void DlgImportCTFile::onAddressImportMethodChanged()
{
  if (m_btnGroupImportAddressMethod->checkedId() == 0)
    m_widgetAddressMethod->setEnabled(true);
  else
    m_widgetAddressMethod->setEnabled(false);
}

void DlgImportCTFile::onBrowseFiles()
{
  QString fileName = QFileDialog::getOpenFileName(this, tr("Open Cheat Table"), QString(),
                                                  tr("Cheat Engine's cheat table (*.CT)"));
  if (fileName != QString())
    m_txbFileName->setText(fileName);
}

void DlgImportCTFile::accept()
{
  if (m_txbFileName->text().isEmpty())
  {
    QMessageBox* errorBox =
        new QMessageBox(QMessageBox::Critical, tr("No Cheat Table file provided"),
                        tr("Please provide a Cheat Table file to import."), QMessageBox::Ok, this);
    errorBox->exec();
    return;
  }
  if (m_btnGroupImportAddressMethod->checkedId() == 0)
  {
    if (m_txbCommonBase->text().isEmpty())
    {
      QMessageBox* errorBox = new QMessageBox(
          QMessageBox::Critical, tr("No assumed RAM start address provided"),
          tr("To use the assumed common start address import method for importing the addresses, "
             "you need to provide the assumed start address of the table."),
          QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }

    std::stringstream ss(m_txbCommonBase->text().toStdString());
    ss >> std::hex;
    ss >> m_commonBase;
    if (ss.fail())
    {
      QMessageBox* errorBox = new QMessageBox(
          QMessageBox::Critical, tr("The assumed start address provided is invalid"),
          tr("Please enter a valid hexadecimal number representing the assumed RAM start address."),
          QMessageBox::Ok, this);
      errorBox->exec();
      return;
    }
  }

  if (m_btnGroupImportAddressMethod->checkedId() == 0)
    m_useDolphinPointers = false;
  else
    m_useDolphinPointers = true;
  m_strFileName = m_txbFileName->text();

  setResult(QDialog::Accepted);
  hide();
}
