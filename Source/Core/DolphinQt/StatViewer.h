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
  void getHeadToHeadJSON();
  void Refresh();

  QPushButton* m_refresh_button;
  QTableWidget* m_table_widget;
};
