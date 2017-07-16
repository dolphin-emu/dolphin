// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/QtUtils/ListTabWidget.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QScrollBar>
#include <QStackedWidget>

class OverriddenListWidget : public QListWidget
{
public:
  // We want this widget to have a fixed width that fits all items, unlike a normal QListWidget
  // which adds scrollbars and expands/contracts.
  OverriddenListWidget() { setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred); }
  QSize sizeHint() const override
  {
    int width = sizeHintForColumn(0) + verticalScrollBar()->sizeHint().width() + 2 * frameWidth();
    int height = QListWidget::sizeHint().height();
    return {width, height};
  }

  // Since this is trying to emulate tabs, an item should always be selected. If the selection tries
  // to change to empty, don't acknowledge it.
  void selectionChanged(const QItemSelection& selected, const QItemSelection& deselected) override
  {
    if (selected.indexes().empty())
      return;
    QListWidget::selectionChanged(selected, deselected);
  }
};

ListTabWidget::ListTabWidget()
{
  QHBoxLayout* layout = new QHBoxLayout();
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);

  m_labels = new OverriddenListWidget();
  layout->addWidget(m_labels);
  m_labels->setIconSize(QSize(32, 32));
  m_labels->setMovement(QListView::Static);
  m_labels->setSpacing(0);

  m_display = new QStackedWidget();
  layout->addWidget(m_display);

  connect(m_labels, &QListWidget::currentItemChanged, this,
          [=](QListWidgetItem* current, QListWidgetItem* previous) {
            m_display->setCurrentIndex(m_labels->row(current));
          });
}

int ListTabWidget::addTab(QWidget* page, const QString& label)
{
  QListWidgetItem* button = new QListWidgetItem();
  button->setText(label);
  button->setTextAlignment(Qt::AlignVCenter);
  button->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

  m_labels->addItem(button);
  if (!m_labels->currentItem())
    m_labels->setCurrentItem(button);

  return m_display->addWidget(page);
}

void ListTabWidget::setTabIcon(int index, const QIcon& icon)
{
  if (auto* label = m_labels->item(index))
    label->setIcon(icon);
}

void ListTabWidget::setCurrentIndex(int index)
{
  m_labels->setCurrentRow(index);
}
