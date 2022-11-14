#include "StatViewer.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>

#include "Common/FileUtil.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"
#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"
#include "UICommon/ResourcePack/Manager.h"

StatViewer::StatViewer(QWidget* widget) : QDialog(widget)
{
  CreateWidgets();

  setWindowTitle(tr("Live Stat Viewer"));
  setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

  //m_table_widget->verticalHeader()->setHidden(true);
  m_table_widget->setAlternatingRowColors(true);
  m_table_widget->setColumnCount(3);
  m_table_widget->setHorizontalHeaderLabels({tr("Home Team"), tr("vs"), tr("Away Team")});
  auto* hor_header = m_table_widget->horizontalHeader();

  hor_header->setSectionResizeMode(0, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(1, QHeaderView::Stretch);
  hor_header->setSectionResizeMode(2, QHeaderView::Stretch);

  resize(QSize(900, 600));
}

void StatViewer::CreateWidgets()
{
  auto* layout = new QGridLayout;

  m_table_widget = new QTableWidget;
  m_table_widget->setTabKeyNavigation(false);

  m_refresh_button = new NonDefaultQPushButton(tr("Refresh"));

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok);

  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(m_refresh_button, &QPushButton::clicked, this, &StatViewer::Refresh);

  layout->addWidget(m_table_widget);
  layout->addWidget(m_refresh_button, 1, 1);
  layout->addWidget(buttons, 7, 1, Qt::AlignRight);
  setLayout(layout);
}

void StatViewer::Refresh()
{
  if (!Core::IsRunningAndStarted())
  {
    ModalMessageBox::warning(this, tr("Error"), tr("Please start a game to see stats."));
    return;
  }
  // get stats
  m_table_widget->setColumnCount(3);
  int rowcount = 7;
  m_table_widget->setRowCount(rowcount);
  std::vector<std::string> propArray = {"Goals", "Shots", "Items", "Hits", "Steals", "Super Strikes", "Perfect Passes"};

  std::vector<int> leftSideValues;
  leftSideValues.push_back(Memory::Read_U16(Metadata::addressLeftSideScore));
  leftSideValues.push_back(Memory::Read_U32(Metadata::addressLeftSideShots));
  leftSideValues.push_back(Memory::Read_U32(Metadata::addressLeftTeamItemCount));
  leftSideValues.push_back(Memory::Read_U16(Metadata::addressLeftSideHits));
  leftSideValues.push_back(Memory::Read_U16(Metadata::addressLeftSideSteals));
  leftSideValues.push_back(Memory::Read_U16(Metadata::addressLeftSideSuperStrikes));
  leftSideValues.push_back(Memory::Read_U16(Metadata::addressLeftSidePerfectPasses));

  std::vector<int> rightSideValues;
  rightSideValues.push_back(Memory::Read_U16(Metadata::addressRightSideScore));
  rightSideValues.push_back(Memory::Read_U32(Metadata::addressRightSideShots));
  rightSideValues.push_back(Memory::Read_U32(Metadata::addressRightTeamItemCount));
  rightSideValues.push_back(Memory::Read_U16(Metadata::addressRightSideHits));
  rightSideValues.push_back(Memory::Read_U16(Metadata::addressRightSideSteals));
  rightSideValues.push_back(Memory::Read_U16(Metadata::addressRightSideSuperStrikes));
  rightSideValues.push_back(Memory::Read_U16(Metadata::addressRightSidePerfectPasses));

  for (int i = 0; i < rowcount; i++)
  {
    auto* leftSide = new QTableWidgetItem(QString::fromStdString(std::to_string(leftSideValues[i])));
    auto* prop = new QTableWidgetItem(QString::fromStdString(propArray[i]));
    auto* rightSide = new QTableWidgetItem(QString::fromStdString(std::to_string(rightSideValues[i])));

    leftSide->setTextAlignment(Qt::AlignCenter);
    prop->setTextAlignment(Qt::AlignCenter);
    rightSide->setTextAlignment(Qt::AlignCenter);

    if (leftSideValues[i] > rightSideValues[i])
    {
      leftSide->setBackground(Qt::GlobalColor::yellow);
      rightSide->setBackground(Qt::GlobalColor::white);
    }
    else if (rightSideValues[i] > leftSideValues[i])
    {
      rightSide->setBackground(Qt::GlobalColor::yellow);
      leftSide->setBackground(Qt::GlobalColor::white);
    }

    m_table_widget->setItem(i, 0, leftSide);
    m_table_widget->setItem(i, 1, prop);
    m_table_widget->setItem(i, 2, rightSide);
  }
  return;
}
