#include "MemScanWidget.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QRadioButton>
#include <QRegExp>
#include <QShortcut>
#include <QVBoxLayout>

#include "../GUICommon.h"

MemScanWidget::MemScanWidget()
{
  initialiseWidgets();
  makeLayouts();
}

MemScanWidget::~MemScanWidget()
{
  delete m_memScanner;
  delete m_resultsListModel;
}

void MemScanWidget::initialiseWidgets()
{
  m_memScanner = new MemScanner();

  m_resultsListModel = new ResultsListModel(this, m_memScanner);

  m_lblResultCount = new QLabel();
  m_tblResulstList = new QTableView();
  m_tblResulstList->setModel(m_resultsListModel);
  m_tblResulstList->setSelectionMode(QAbstractItemView::ExtendedSelection);

  m_tblResulstList->horizontalHeader()->setStretchLastSection(true);
  m_tblResulstList->horizontalHeader()->resizeSection(ResultsListModel::RESULT_COL_ADDRESS, 125);
  m_tblResulstList->horizontalHeader()->resizeSection(ResultsListModel::RESULT_COL_SCANNED, 150);

  m_tblResulstList->setSelectionBehavior(QAbstractItemView::SelectRows);
  m_tblResulstList->setSelectionMode(QAbstractItemView::ExtendedSelection);
  m_tblResulstList->setMinimumWidth(385);
  connect(m_tblResulstList, &QAbstractItemView::doubleClicked, this,
          &MemScanWidget::onResultListDoubleClicked);

  m_btnAddAll = new QPushButton(tr("Add all"));
  connect(m_btnAddAll, &QPushButton::clicked, this, &MemScanWidget::onAddAll);
  m_btnAddAll->setEnabled(false);

  m_btnAddSelection = new QPushButton(tr("Add selection"));
  connect(m_btnAddSelection, &QPushButton::clicked, this, &MemScanWidget::onAddSelection);
  m_btnAddSelection->setEnabled(false);

  m_btnRemoveSelection = new QPushButton(tr("Remove selection"));
  connect(m_btnRemoveSelection, &QPushButton::clicked, this, &MemScanWidget::onRemoveSelection);
  m_btnRemoveSelection->setEnabled(false);

  m_btnFirstScan = new QPushButton(tr("First scan"));
  m_btnNextScan = new QPushButton(tr("Next scan"));
  m_btnNextScan->hide();
  m_btnResetScan = new QPushButton(tr("Reset scan"));
  m_btnResetScan->hide();

  connect(m_btnFirstScan, &QPushButton::clicked, this, &MemScanWidget::onFirstScan);
  connect(m_btnNextScan, &QPushButton::clicked, this, &MemScanWidget::onNextScan);
  connect(m_btnResetScan, &QPushButton::clicked, this, &MemScanWidget::onResetScan);

  QShortcut* scanShortcut = new QShortcut(QKeySequence(Qt::Key::Key_Enter), this);
  connect(scanShortcut, &QShortcut::activated, this, [=] {
    if (m_memScanner->hasScanStarted())
      onNextScan();
    else
      onFirstScan();
  });

  m_txbSearchTerm1 = new QLineEdit();
  m_txbSearchTerm2 = new QLineEdit();

  m_searchTerm2Widget = new QWidget();

  m_searchTerm2Widget->hide();

  m_cmbScanType = new QComboBox();
  m_cmbScanType->addItems(GUICommon::g_memTypeNames);
  m_cmbScanType->setCurrentIndex(0);
  connect(m_cmbScanType, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MemScanWidget::onScanMemTypeChanged);
  m_variableLengthType = false;

  m_cmbScanFilter = new QComboBox();
  updateScanFilterChoices();
  connect(m_cmbScanFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          &MemScanWidget::onScanFilterChanged);

  m_rdbBaseDecimal = new QRadioButton(tr("Decimal"));
  m_rdbBaseHexadecimal = new QRadioButton(tr("Hexadecimal"));
  m_rdbBaseOctal = new QRadioButton(tr("Octal"));
  m_rdbBaseBinary = new QRadioButton(tr("Binary"));

  m_btnGroupScanBase = new QButtonGroup(this);
  m_btnGroupScanBase->addButton(m_rdbBaseDecimal, 0);
  m_btnGroupScanBase->addButton(m_rdbBaseHexadecimal, 1);
  m_btnGroupScanBase->addButton(m_rdbBaseOctal, 2);
  m_btnGroupScanBase->addButton(m_rdbBaseBinary, 3);
  m_rdbBaseDecimal->setChecked(true);

  m_groupScanBase = new QGroupBox(tr("Base to use"));

  m_chkSignedScan = new QCheckBox(tr("Signed value scan"));
  m_chkSignedScan->setChecked(false);

  m_chkEnforceMemAlignement = new QCheckBox(tr("Enforce alignement"));
  m_chkEnforceMemAlignement->setChecked(true);
}

void MemScanWidget::makeLayouts()
{
  QLabel* lblAnd = new QLabel(tr("and"));

  QHBoxLayout* multiAddButtons_layout = new QHBoxLayout();
  multiAddButtons_layout->addWidget(m_btnAddSelection);
  multiAddButtons_layout->addWidget(m_btnAddAll);
  multiAddButtons_layout->addWidget(m_btnRemoveSelection);

  QVBoxLayout* results_layout = new QVBoxLayout();
  results_layout->addWidget(m_lblResultCount);
  results_layout->addWidget(m_tblResulstList);
  results_layout->addLayout(multiAddButtons_layout);

  QHBoxLayout* buttons_layout = new QHBoxLayout();
  buttons_layout->addWidget(m_btnFirstScan);
  buttons_layout->addWidget(m_btnNextScan);
  buttons_layout->addWidget(m_btnResetScan);

  QHBoxLayout* searchTerm2_layout = new QHBoxLayout();
  searchTerm2_layout->setContentsMargins(0, 0, 0, 0);
  searchTerm2_layout->addWidget(lblAnd);
  searchTerm2_layout->addWidget(m_txbSearchTerm2);
  m_searchTerm2Widget->setLayout(searchTerm2_layout);

  QHBoxLayout* searchTerms_layout = new QHBoxLayout();
  searchTerms_layout->addWidget(m_txbSearchTerm1);
  searchTerms_layout->addWidget(m_searchTerm2Widget);
  searchTerms_layout->setSizeConstraint(QLayout::SetMinimumSize);

  QHBoxLayout* layout_buttonsBase = new QHBoxLayout();
  layout_buttonsBase->addWidget(m_rdbBaseDecimal);
  layout_buttonsBase->addWidget(m_rdbBaseHexadecimal);
  layout_buttonsBase->addWidget(m_rdbBaseOctal);
  layout_buttonsBase->addWidget(m_rdbBaseBinary);
  m_groupScanBase->setLayout(layout_buttonsBase);

  QHBoxLayout* layout_extraParams = new QHBoxLayout();
  layout_extraParams->addWidget(m_chkEnforceMemAlignement);
  layout_extraParams->addWidget(m_chkSignedScan);

  QVBoxLayout* scannerParams_layout = new QVBoxLayout();
  scannerParams_layout->addLayout(buttons_layout);
  scannerParams_layout->addWidget(m_cmbScanType);
  scannerParams_layout->addWidget(m_cmbScanFilter);
  scannerParams_layout->addLayout(searchTerms_layout);
  scannerParams_layout->addWidget(m_groupScanBase);
  scannerParams_layout->addLayout(layout_extraParams);
  scannerParams_layout->addStretch();
  scannerParams_layout->setContentsMargins(0, 0, 0, 0);

  QWidget* scannerParamsWidget = new QWidget();
  scannerParamsWidget->setLayout(scannerParams_layout);
  scannerParamsWidget->setMinimumWidth(425);

  QHBoxLayout* main_layout = new QHBoxLayout();
  main_layout->addLayout(results_layout);
  main_layout->addWidget(scannerParamsWidget);
  main_layout->setContentsMargins(3, 0, 3, 0);

  setLayout(main_layout);
}

ResultsListModel* MemScanWidget::getResultListModel() const
{
  return m_resultsListModel;
}

std::vector<u32> MemScanWidget::getAllResults() const
{
  return m_memScanner->getResultsConsoleAddr();
}

QModelIndexList MemScanWidget::getSelectedResults() const
{
  return m_tblResulstList->selectionModel()->selectedRows();
}

MemScanner::ScanFiter MemScanWidget::getSelectedFilter() const
{
  int index =
      GUICommon::g_memScanFilter.indexOf(QRegExp(tr("^") + m_cmbScanFilter->currentText() + tr("$")));
  return static_cast<MemScanner::ScanFiter>(index);
}

void MemScanWidget::updateScanFilterChoices()
{
  Common::MemType newType = static_cast<Common::MemType>(m_cmbScanType->currentIndex());
  m_cmbScanFilter->clear();
  if (newType == Common::MemType::type_byteArray || newType == Common::MemType::type_string)
  {
    m_cmbScanFilter->addItem(
        GUICommon::g_memScanFilter.at(static_cast<int>(MemScanner::ScanFiter::exact)));
  }
  else if (m_memScanner->hasScanStarted())
  {
    m_cmbScanFilter->addItems(GUICommon::g_memScanFilter);
    m_cmbScanFilter->removeItem(static_cast<int>(MemScanner::ScanFiter::unknownInitial));
  }
  else
  {
    m_cmbScanFilter->addItem(
        GUICommon::g_memScanFilter.at(static_cast<int>(MemScanner::ScanFiter::exact)));
    m_cmbScanFilter->addItem(
        GUICommon::g_memScanFilter.at(static_cast<int>(MemScanner::ScanFiter::between)));
    m_cmbScanFilter->addItem(
        GUICommon::g_memScanFilter.at(static_cast<int>(MemScanner::ScanFiter::biggerThan)));
    m_cmbScanFilter->addItem(
        GUICommon::g_memScanFilter.at(static_cast<int>(MemScanner::ScanFiter::smallerThan)));
    m_cmbScanFilter->addItem(
        GUICommon::g_memScanFilter.at(static_cast<int>(MemScanner::ScanFiter::unknownInitial)));
  }
  m_cmbScanFilter->setCurrentIndex(0);
}

void MemScanWidget::updateTypeAdditionalOptions()
{
  if (m_memScanner->typeSupportsAdditionalOptions(
          static_cast<Common::MemType>(m_cmbScanType->currentIndex())))
  {
    m_chkSignedScan->show();
    m_groupScanBase->show();
  }
  else
  {
    m_chkSignedScan->hide();
    m_groupScanBase->hide();
  }
}

void MemScanWidget::onScanFilterChanged()
{
  MemScanner::ScanFiter theFilter = getSelectedFilter();
  int numTerms = m_memScanner->getTermsNumForFilter(theFilter);
  switch (numTerms)
  {
  case 0:
    m_txbSearchTerm1->hide();
    m_searchTerm2Widget->hide();
    m_chkSignedScan->hide();
    m_groupScanBase->hide();
    break;
  case 1:
    m_txbSearchTerm1->show();
    m_searchTerm2Widget->hide();
    updateTypeAdditionalOptions();
    break;
  case 2:
    m_txbSearchTerm1->show();
    m_searchTerm2Widget->show();
    updateTypeAdditionalOptions();
    break;
  }
}

void MemScanWidget::onScanMemTypeChanged()
{
  Common::MemType newType = static_cast<Common::MemType>(m_cmbScanType->currentIndex());
  if (!m_variableLengthType &&
      (newType == Common::MemType::type_string || newType == Common::MemType::type_byteArray))
  {
    updateScanFilterChoices();
    m_variableLengthType = true;
  }
  else if (m_variableLengthType && newType != Common::MemType::type_string &&
           newType != Common::MemType::type_byteArray)
  {
    updateScanFilterChoices();
    m_variableLengthType = false;
  }

  updateTypeAdditionalOptions();
}

void MemScanWidget::onFirstScan()
{
  m_memScanner->setType(static_cast<Common::MemType>(m_cmbScanType->currentIndex()));
  m_memScanner->setIsSigned(m_chkSignedScan->isChecked());
  m_memScanner->setEnforceMemAlignement(m_chkEnforceMemAlignement->isChecked());
  m_memScanner->setBase(static_cast<Common::MemBase>(m_btnGroupScanBase->checkedId()));
  Common::MemOperationReturnCode scannerReturn =
      m_memScanner->firstScan(getSelectedFilter(), m_txbSearchTerm1->text().toStdString(),
                              m_txbSearchTerm2->text().toStdString());
  if (scannerReturn != Common::MemOperationReturnCode::OK)
  {
    handleScannerErrors(scannerReturn);
  }
  else
  {
    int resultsFound = static_cast<int>(m_memScanner->getResultCount());
    m_lblResultCount->setText(
        tr("%1 result(s) found", "", resultsFound).arg(QString::number(resultsFound)));
    if (resultsFound <= 1000 && resultsFound != 0)
    {
      m_btnAddAll->setEnabled(true);
      m_btnAddSelection->setEnabled(true);
      m_btnRemoveSelection->setEnabled(true);
    }
    m_btnFirstScan->hide();
    m_btnNextScan->show();
    m_btnResetScan->show();
    m_cmbScanType->setDisabled(true);
    m_chkSignedScan->setDisabled(true);
    m_chkEnforceMemAlignement->setDisabled(true);
    m_groupScanBase->setDisabled(true);
    updateScanFilterChoices();
  }
}

void MemScanWidget::onNextScan()
{
  Common::MemOperationReturnCode scannerReturn =
      m_memScanner->nextScan(getSelectedFilter(), m_txbSearchTerm1->text().toStdString(),
                             m_txbSearchTerm2->text().toStdString());
  if (scannerReturn != Common::MemOperationReturnCode::OK)
  {
    handleScannerErrors(scannerReturn);
  }
  else
  {
    int resultsFound = static_cast<int>(m_memScanner->getResultCount());
    m_lblResultCount->setText(
        tr("%1 result(s) found", "", resultsFound).arg(QString::number(resultsFound)));
    if (resultsFound <= 1000 && resultsFound != 0)
    {
      m_btnAddAll->setEnabled(true);
      m_btnAddSelection->setEnabled(true);
      m_btnRemoveSelection->setEnabled(true);
    }
  }
}

void MemScanWidget::onResetScan()
{
  m_memScanner->reset();
  m_lblResultCount->setText(QString());
  m_btnAddAll->setEnabled(false);
  m_btnAddSelection->setEnabled(false);
  m_btnRemoveSelection->setEnabled(false);
  m_btnFirstScan->show();
  m_btnNextScan->hide();
  m_btnResetScan->hide();
  m_cmbScanType->setEnabled(true);
  m_chkSignedScan->setEnabled(true);
  m_chkEnforceMemAlignement->setEnabled(true);
  m_groupScanBase->setEnabled(true);
  m_resultsListModel->updateAfterScannerReset();
  updateScanFilterChoices();
}

void MemScanWidget::onAddSelection()
{
  emit requestAddSelectedResultsToWatchList(m_memScanner->getType(), m_memScanner->getLength(),
                                            m_memScanner->getIsUnsigned(), m_memScanner->getBase());
}

void MemScanWidget::onRemoveSelection()
{
  if (!m_tblResulstList->selectionModel()->hasSelection())
    return;

  while (m_tblResulstList->selectionModel()->hasSelection())
    m_resultsListModel->removeRow(m_tblResulstList->selectionModel()->selectedRows().at(0).row());

  // The result count is already updated at the backend by this point
  int resultsFound = static_cast<int>(m_memScanner->getResultCount());
  m_lblResultCount->setText(
      tr("%1 result(s) found", "", resultsFound).arg(QString::number(resultsFound)));
}

void MemScanWidget::onAddAll()
{
  emit requestAddAllResultsToWatchList(m_memScanner->getType(), m_memScanner->getLength(),
                                       m_memScanner->getIsUnsigned(), m_memScanner->getBase());
}

void MemScanWidget::handleScannerErrors(const Common::MemOperationReturnCode errorCode)
{
  if (errorCode == Common::MemOperationReturnCode::invalidInput)
  {
    QMessageBox* errorBox =
        new QMessageBox(QMessageBox::Critical, tr("Invalid term(s)"),
                        tr("The search term(s) you entered for the type %1 is/are invalid")
                            .arg(m_cmbScanType->currentText()),
                        QMessageBox::Ok, this);
    errorBox->exec();
  }
  else if(errorCode == Common::MemOperationReturnCode::uninitialized)
  {
	  QMessageBox* errorBox =
        new QMessageBox(QMessageBox::Critical, tr("Invalid memory operation"),
                        tr("Memory is not initialized.")
                            .arg(m_cmbScanType->currentText()),
                        QMessageBox::Ok, this);
    errorBox->exec();

  }
}

void MemScanWidget::onResultListDoubleClicked(const QModelIndex& index)
{
  if (index != QVariant())
  {
    emit requestAddWatchEntry(m_resultsListModel->getResultAddress(index.row()),
                              m_memScanner->getType(), m_memScanner->getLength(),
                              m_memScanner->getIsUnsigned(), m_memScanner->getBase());
  }
}
