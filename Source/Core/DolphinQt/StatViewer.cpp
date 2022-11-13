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

  resize(QSize(900, 600));
}

void StatViewer::CreateWidgets()
{
  auto* layout = new QGridLayout;

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok);

  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

  layout->addWidget(buttons, 7, 1, Qt::AlignRight);
}
