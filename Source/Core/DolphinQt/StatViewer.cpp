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

#include <string>

#include <fmt/format.h>
#include <picojson.h>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/HttpRequest.h"
#include "Common/Logging/Log.h"
#include <Core/HW/AddressSpace.h>

struct StatValue
{
  std::string displayValue;
  int numericalValue;
};

static picojson::array jsonResult;

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

void StatViewer::getNetPlayNamesHeadtoHead()
{

}

void StatViewer::getHeadToHeadJSON(std::string p1Name, std::string p2Name)
{
  // This url returns a json containing info with the players rated head to head results
  std::string url = "https://api.mariostrikers.gg/ratings/h2h?gametype=2&p1=" + p1Name + "&p2=" + p2Name;
  Common::HttpRequest::Headers headers = {
      {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like "
                     "Gecko) Chrome/97.0.4692.71 Safari/537.36"}};

  Common::HttpRequest req{std::chrono::seconds{3}};
  auto resp = req.Get(url, headers);
  if (!resp)
  {
    // null out the jsonResult in case we used to have something and now we don't
    picojson::array tempArray;
    jsonResult = tempArray;
    return;
  }
  const std::string contents(reinterpret_cast<char*>(resp->data()), resp->size());

  picojson::value json;
  const std::string err = picojson::parse(json, contents);
  if (!err.empty())
  {
    // null out the jsonResult in case we used to have something and now we don't
    picojson::array tempArray;
    jsonResult = tempArray;
    return;
  }
  jsonResult = json.get<picojson::array>();
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
  int rowcount = 10;
  bool markedNameandWinsRow = false;
  std::vector<StatValue> leftSideValues;
  std::vector<StatValue> rightSideValues;
  std::vector<std::string> propArray = {"Name / Rated Wins Against Opp", "Goals",  "Shots", "Goals to Shots Ratio", "Items", "Hits",
                                        "Steals", "Super Strikes", "Perfect Passes", "Time of Possession"};
  // determine if in netplay and other match props and set row count accordingly. should be one less
  // than norm
  bool leftTeamNetPlayFound = false;
  bool rightTeamNetPlayFound = false;

  int leftTeamNetPlayPort = -1;
  int rightTeamNetPlayPort = -1;

  std::string leftTeamNetPlayName = "";
  std::string rightTeamNetPlayName = "";

  if (NetPlay::IsNetPlayRunning())
  {
    int currentPortValue = Memory::Read_U16(Metadata::addressControllerPort1);
    if (currentPortValue == 0)
    {
      leftTeamNetPlayPort = 0;
      leftTeamNetPlayFound = true;
    }
    else if (currentPortValue == 1)
    {
      rightTeamNetPlayPort = 0;
      rightTeamNetPlayFound = true;
    }

    currentPortValue = Memory::Read_U16(Metadata::addressControllerPort2);
    if (currentPortValue == 0)
    {
      leftTeamNetPlayPort = 1;
      leftTeamNetPlayFound = true;
    }
    else if (currentPortValue == 1)
    {
      rightTeamNetPlayPort = 1;
      rightTeamNetPlayFound = true;
    }

    // just check this to save read from mem time
    if (!leftTeamNetPlayFound && !rightTeamNetPlayFound)
    {
      currentPortValue = Memory::Read_U16(Metadata::addressControllerPort3);
      if (currentPortValue == 0)
      {
        leftTeamNetPlayPort = 2;
        leftTeamNetPlayFound = true;
      }
      else if (currentPortValue == 1)
      {
        rightTeamNetPlayPort = 2;
        rightTeamNetPlayFound = true;
      }
    }

    // just check this to save read from mem time
    if (!leftTeamNetPlayFound && !rightTeamNetPlayFound)
    {
      currentPortValue = Memory::Read_U16(Metadata::addressControllerPort4);
      if (currentPortValue == 0)
      {
        leftTeamNetPlayPort = 3;
        leftTeamNetPlayFound = true;
      }
      else if (currentPortValue == 1)
      {
        rightTeamNetPlayPort = 3;
        rightTeamNetPlayFound = true;
      }
    }

    if (leftTeamNetPlayFound && rightTeamNetPlayFound)
    {
      // netplay is running and we have at least one player on both sides
      // therefore, we can get their player ids from pad map now since we know the ports
      // using the id's we can get their player names

      std::vector<const NetPlay::Player*> playerArray = Metadata::getPlayerArray();
      NetPlay::PadMappingArray netplayGCMap = Metadata::getControllers();

      // getting left team
      // so we're going to go to m_pad_map and find what player id is at port 0
      // then we're going to search for that player id in our player array and return their name
      NetPlay::PlayerId pad_map_player_id = netplayGCMap[leftTeamNetPlayPort];
      // now get the player name that matches the PID we just stored
      for (int j = 0; j < playerArray.size(); j++)
      {
        if (playerArray.at(j)->pid == pad_map_player_id)
        {
          leftTeamNetPlayName = playerArray.at(j)->name;
          break;
        }
      }

      // getting right team
      pad_map_player_id = netplayGCMap[rightTeamNetPlayPort];
      for (int j = 0; j < playerArray.size(); j++)
      {
        if (playerArray.at(j)->pid == pad_map_player_id)
        {
          rightTeamNetPlayName = playerArray.at(j)->name;
          break;
        }
      }
      getHeadToHeadJSON(leftTeamNetPlayName, rightTeamNetPlayName);

      picojson::array::iterator it;
      StatValue leftNetPlayNameWins;
      StatValue rightNetPlayNameWins;
      if (jsonResult.empty())
      {
        leftNetPlayNameWins = {leftTeamNetPlayName + " - " + "N/A Wins", 0};
        leftSideValues.push_back(leftNetPlayNameWins);
        rightNetPlayNameWins = {rightTeamNetPlayName + " - " + "N/A Wins", 0};
        rightSideValues.push_back(rightNetPlayNameWins);
      }
      else
      {
        for (it = jsonResult.begin(); it != jsonResult.end(); it++)
        {
          picojson::object obj = it->get<picojson::object>();
          leftNetPlayNameWins = {"N/A", 0};
          leftNetPlayNameWins.displayValue =
              leftTeamNetPlayName + " -" + obj["p1wins"].to_str() + " Wins";
          leftNetPlayNameWins.numericalValue = std::stoi(leftNetPlayNameWins.displayValue);
          leftSideValues.push_back(leftNetPlayNameWins);
          // we push these back so that the for loop at the bottom that iterates through all
          // the other rows is aligned correctly

          rightNetPlayNameWins = {"N/A", 0};
          rightNetPlayNameWins.displayValue =
              rightTeamNetPlayName + " -" + obj["p2wins"].to_str() + " Wins";
          rightNetPlayNameWins.numericalValue = std::stoi(rightNetPlayNameWins.displayValue);
          rightSideValues.push_back(rightNetPlayNameWins);
        }
      }
      auto* leftSide =
          new QTableWidgetItem(QString::fromStdString(leftNetPlayNameWins.displayValue));
      auto* prop = new QTableWidgetItem(QString::fromStdString("Name / Rated Head-to-Head Wins"));
      auto* rightSide =
          new QTableWidgetItem(QString::fromStdString(rightNetPlayNameWins.displayValue));

      leftSide->setTextAlignment(Qt::AlignCenter);
      prop->setTextAlignment(Qt::AlignCenter);
      rightSide->setTextAlignment(Qt::AlignCenter);

      m_table_widget->setItem(0, 0, leftSide);
      m_table_widget->setItem(0, 1, prop);
      m_table_widget->setItem(0, 2, rightSide);
      markedNameandWinsRow = true;
    }
    else
    {
      propArray.erase(propArray.begin());
      rowcount = 9;
      markedNameandWinsRow = false;
    }
  }
  else
  {
    propArray.erase(propArray.begin());
    rowcount = 9;
    markedNameandWinsRow = false;
  }

  m_table_widget->setRowCount(rowcount);

  // ---- Pre-Calculated Possession Time Values ---- //
  const AddressSpace::Accessors* accessors =
      AddressSpace::GetAccessors(AddressSpace::Type::Effective);
  u32 leftSideBallOwnedFrames = Memory::Read_U32(Metadata::addressLeftTeamBallOwnedFrames);
  u32 rightSideBallOwnedFrames = Memory::Read_U32(Metadata::addressRightTeamBallOwnedFrames);
  float totalBallOwnedFrames = leftSideBallOwnedFrames + rightSideBallOwnedFrames;
  u32 matchTimeElapsed = accessors->ReadF32(Metadata::addressTimeElapsed);
  // formula goes side frames owned / total frames. this gets a percent. percent times time elapsed
  // gets you time owned. time owned modulus 60 (60 seconds in a minute, not related to fps) gets
  // you seconds figure. time owned / 60 gets you minute figure.
  float leftSideBallOwnedFramesPercent = leftSideBallOwnedFrames / totalBallOwnedFrames;
  float rightSideBallOwnedFramesPercent = rightSideBallOwnedFrames / totalBallOwnedFrames;

  float leftSideBallOwnedFramesTotalSeconds = leftSideBallOwnedFramesPercent * matchTimeElapsed;
  float rightSideBallOwnedFramesTotalSeconds = rightSideBallOwnedFramesPercent * matchTimeElapsed;

  int leftSideBallOwnedFramesMinutes = leftSideBallOwnedFramesTotalSeconds / 60;
  int rightSideBallOwnedFramesMinutes = rightSideBallOwnedFramesTotalSeconds / 60;

  int leftSideBallOwnedFramesSeconds = (int)leftSideBallOwnedFramesTotalSeconds % 60;
  int rightSideBallOwnedFramesSeconds = (int)rightSideBallOwnedFramesTotalSeconds % 60;
  std::string leftSideBallOwnedFramesSecondsString;
  std::string rightSideBallOwnedFramesSecondsString;
  if (leftSideBallOwnedFramesSeconds > 9)
  {
    leftSideBallOwnedFramesSecondsString = std::to_string(leftSideBallOwnedFramesSeconds);
  }
  else
  {
    leftSideBallOwnedFramesSecondsString = "0" + std::to_string(leftSideBallOwnedFramesSeconds);
  }

  if (rightSideBallOwnedFramesSeconds > 9)
  {
    rightSideBallOwnedFramesSecondsString = std::to_string(rightSideBallOwnedFramesSeconds);
  }
  else
  {
    rightSideBallOwnedFramesSecondsString = "0" + std::to_string(rightSideBallOwnedFramesSeconds);
  }

  // clean up percent to make it a rounded whole number
  leftSideBallOwnedFramesPercent = std::round(leftSideBallOwnedFramesPercent * 100);
  rightSideBallOwnedFramesPercent = std::round(rightSideBallOwnedFramesPercent * 100);


  // ----- Left Values ----- //
  float leftSideScore = Memory::Read_U16(Metadata::addressLeftSideScore);
  StatValue ourValue = {std::to_string((int)leftSideScore),
                        (int)leftSideScore};
  leftSideValues.push_back(ourValue);

  float leftSideShots = Memory::Read_U32(Metadata::addressLeftSideShots);
  ourValue.displayValue = std::to_string((int)leftSideShots);
  ourValue.numericalValue = leftSideShots;
  leftSideValues.push_back(ourValue);

  if ((int)leftSideShots == 0)
  {
    ourValue.displayValue = "0/0 (0%)";
    ourValue.numericalValue = 0;
  }
  else
  {
    float leftSideShotToGoalRatio = std::round((leftSideScore / leftSideShots) * 100);
    ourValue.displayValue = std::to_string((int)leftSideScore) + "/" + std::to_string((int)leftSideShots) +
                            " (" + std::to_string((int)leftSideShotToGoalRatio) + "%)";
    ourValue.numericalValue = leftSideShotToGoalRatio;
  }
  leftSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U32(Metadata::addressLeftTeamItemCount));
  ourValue.numericalValue = Memory::Read_U32(Metadata::addressLeftTeamItemCount);
  leftSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U16(Metadata::addressLeftSideHits));
  ourValue.numericalValue = Memory::Read_U16(Metadata::addressLeftSideHits);
  leftSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U16(Metadata::addressLeftSideSteals));
  ourValue.numericalValue = Memory::Read_U16(Metadata::addressLeftSideSteals);
  leftSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U16(Metadata::addressLeftSideSuperStrikes));
  ourValue.numericalValue = Memory::Read_U16(Metadata::addressLeftSideSuperStrikes);
  leftSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U16(Metadata::addressLeftSidePerfectPasses));
  ourValue.numericalValue = Memory::Read_U16(Metadata::addressLeftSidePerfectPasses);
  leftSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(leftSideBallOwnedFramesMinutes) + ":" +
                          leftSideBallOwnedFramesSecondsString + " (" +
                          std::to_string(int(leftSideBallOwnedFramesPercent)) + "%)";
  ourValue.numericalValue = Memory::Read_U32(Metadata::addressLeftTeamBallOwnedFrames);
  leftSideValues.push_back(ourValue);


  // ------ Right Values ------ //
  float rightSideScore = Memory::Read_U16(Metadata::addressRightSideScore);
  ourValue = {std::to_string((int)rightSideScore), (int)rightSideScore};
  rightSideValues.push_back(ourValue);

  float rightSideShots = Memory::Read_U32(Metadata::addressRightSideShots);
  ourValue.displayValue = std::to_string((int)rightSideShots);
  ourValue.numericalValue = rightSideShots;
  rightSideValues.push_back(ourValue);

  if ((int)rightSideShots == 0)
  {
    ourValue.displayValue = "0/0 (0%)";
    ourValue.numericalValue = 0;
  }
  else
  {
    float rightSideShotToGoalRatio = std::round((rightSideScore / rightSideShots) * 100);
    ourValue.displayValue = std::to_string((int)rightSideScore) + "/" + std::to_string((int)rightSideShots) +
                            " (" + std::to_string((int)rightSideShotToGoalRatio) + "%)";
    ourValue.numericalValue = rightSideShotToGoalRatio;
  }
  rightSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U32(Metadata::addressRightTeamItemCount));
  ourValue.numericalValue = Memory::Read_U32(Metadata::addressRightTeamItemCount);
  rightSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U16(Metadata::addressRightSideHits));
  ourValue.numericalValue = Memory::Read_U16(Metadata::addressRightSideHits);
  rightSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U16(Metadata::addressRightSideSteals));
  ourValue.numericalValue = Memory::Read_U16(Metadata::addressRightSideSteals);
  rightSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U16(Metadata::addressRightSideSuperStrikes));
  ourValue.numericalValue = Memory::Read_U16(Metadata::addressRightSideSuperStrikes);
  rightSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(Memory::Read_U16(Metadata::addressRightSidePerfectPasses));
  ourValue.numericalValue = Memory::Read_U16(Metadata::addressRightSidePerfectPasses);
  rightSideValues.push_back(ourValue);

  ourValue.displayValue = std::to_string(rightSideBallOwnedFramesMinutes) + ":" +
                          rightSideBallOwnedFramesSecondsString + " (" +
                          std::to_string(int(rightSideBallOwnedFramesPercent)) + "%)";
  ourValue.numericalValue = Memory::Read_U32(Metadata::addressRightTeamBallOwnedFrames);
  rightSideValues.push_back(ourValue);

  for (int i = 0; i < rowcount; i++)
  {
    // we need to skip the first iteration if we did the netplay row
    if (markedNameandWinsRow && (i == 0))
    {
      continue;
    }
    auto* leftSide = new QTableWidgetItem(QString::fromStdString(leftSideValues.at(i).displayValue));
    auto* prop = new QTableWidgetItem(QString::fromStdString(propArray[i]));
    auto* rightSide = new QTableWidgetItem(QString::fromStdString(rightSideValues.at(i).displayValue));

    leftSide->setTextAlignment(Qt::AlignCenter);
    prop->setTextAlignment(Qt::AlignCenter);
    rightSide->setTextAlignment(Qt::AlignCenter);

    if (leftSideValues.at(i).numericalValue > rightSideValues.at(i).numericalValue)
    {
      leftSide->setBackground(Qt::GlobalColor::yellow);
      rightSide->setBackground(Qt::GlobalColor::white);
    }
    else if (rightSideValues.at(i).numericalValue > leftSideValues.at(i).numericalValue)
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
