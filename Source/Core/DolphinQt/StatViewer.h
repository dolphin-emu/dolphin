#pragma once

#include <QDialog>
#include "Core/Core.h"
#include "Core/HW/Memmap.h"
#include "Core/Metadata.h"
#include <Core/NetPlayProto.h>
#include <Core/NetPlayClient.h>

class QPushButton;
class QTableWidget;


class StatViewer : public QDialog
{
public:
  explicit StatViewer(QWidget* parent = nullptr);

private:
  void CreateWidgets();
  void getNetPlayNamesHeadtoHead();
  void getHeadToHeadJSON(std::string p1Name, std::string p2Name);
  void Refresh();

  QPushButton* m_refresh_button;
  QTableWidget* m_table_widget;
};
