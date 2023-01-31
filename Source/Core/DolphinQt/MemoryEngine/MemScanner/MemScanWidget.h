#pragma once

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTableView>
#include <QTimer>
#include <QWidget>

#include "ResultsListModel.h"

class MemScanWidget : public QWidget
{
  Q_OBJECT

public:
  MemScanWidget();
  ~MemScanWidget();

  ResultsListModel* getResultListModel() const;
  std::vector<u32> getAllResults() const;
  QModelIndexList getSelectedResults() const;

  void onScanFilterChanged();
  void onScanMemTypeChanged();
  void onResultListDoubleClicked(const QModelIndex& index);
  void handleScannerErrors(const Common::MemOperationReturnCode errorCode);
  void onFirstScan();
  void onNextScan();
  void onResetScan();
  void onAddSelection();
  void onRemoveSelection();
  void onAddAll();
  QTimer* getUpdateTimer() const;

signals:
  void requestAddWatchEntry(u32 address, Common::MemType type, size_t length, bool isUnsigned,
                            Common::MemBase base);
  void requestAddSelectedResultsToWatchList(Common::MemType type, size_t length, bool isUnsigned,
                                            Common::MemBase base);
  void requestAddAllResultsToWatchList(Common::MemType type, size_t length, bool isUnsigned,
                                       Common::MemBase base);
  void mustUnhook();

private:
  void initialiseWidgets();
  void makeLayouts();

  MemScanner::ScanFiter getSelectedFilter() const;
  void updateScanFilterChoices();
  void updateTypeAdditionalOptions();

  MemScanner* m_memScanner;
  ResultsListModel* m_resultsListModel;
  QPushButton* m_btnFirstScan;
  QPushButton* m_btnNextScan;
  QPushButton* m_btnResetScan;
  QPushButton* m_btnAddSelection;
  QPushButton* m_btnAddAll;
  QPushButton* m_btnRemoveSelection;
  QLineEdit* m_txbSearchTerm1;
  QLineEdit* m_txbSearchTerm2;
  QWidget* m_searchTerm2Widget;
  QTimer* m_currentValuesUpdateTimer;
  QComboBox* m_cmbScanFilter;
  QComboBox* m_cmbScanType;
  QLabel* m_lblResultCount;
  QCheckBox* m_chkSignedScan;
  QCheckBox* m_chkEnforceMemAlignement;
  QButtonGroup* m_btnGroupScanBase;
  QRadioButton* m_rdbBaseDecimal;
  QRadioButton* m_rdbBaseHexadecimal;
  QRadioButton* m_rdbBaseOctal;
  QRadioButton* m_rdbBaseBinary;
  QGroupBox* m_groupScanBase;
  QTableView* m_tblResulstList;
  bool m_variableLengthType;
};
