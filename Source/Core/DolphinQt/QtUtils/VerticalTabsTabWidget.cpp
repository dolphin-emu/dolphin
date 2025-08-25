// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/VerticalTabsTabWidget.h"

#include <QApplication>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <qstringliteral.h>

namespace
{
class VerticalTabBar : public QTabBar
{
public:
  explicit VerticalTabBar(QWidget* const parent = nullptr) : QTabBar(parent) { setDrawBase(false); }

  void UseVerticalTabs(const bool vertical_tabs)
  {
    m_vertical_tabs = vertical_tabs;
    setUsesScrollButtons(!vertical_tabs);
  }

protected:
  QSize tabSizeHint(const int index) const override
  {
    if (!m_vertical_tabs)
      return QTabBar::tabSizeHint(index);

    return QTabBar::tabSizeHint(index).transposed();
  }

  void paintEvent(QPaintEvent* const event) override
  {
    if (!m_vertical_tabs)
      return QTabBar::paintEvent(event);

    QStylePainter painter(this);

    const int current_tab_index{currentIndex()};

    const int tab_count{count()};
    for (int i{0}; i < tab_count; ++i)
    {
      if (i != current_tab_index)
        PaintTab(painter, i);
    }

    // Current tab is painted last as, depending on the [system] style, it is possible that the
    // decoration is required to occlude the adjacent tab underneath.
    PaintTab(painter, current_tab_index);
  }

private:
  void PaintTab(QStylePainter& painter, const int tab_index)
  {
    painter.save();

    QStyleOptionTab option;
    initStyleOption(&option, tab_index);
    painter.drawControl(QStyle::CE_TabBarTabShape, option);

    const QSize size{option.rect.size().transposed()};
    QRect rect(QPoint(), size);
    rect.moveCenter(option.rect.center());
    option.rect = rect;

    const QPoint center{tabRect(tab_index).center()};
    painter.translate(center);
    painter.rotate(90.0);
    painter.translate(-center);
    painter.drawControl(QStyle::CE_TabBarTabLabel, option);

    painter.restore();
  }

  bool m_vertical_tabs{false};
};
}  // namespace

namespace QtUtils
{

VerticalTabsTabWidget::VerticalTabsTabWidget(QWidget* const parent) : QTabWidget(parent)
{
  setTabBar(new VerticalTabBar);
  UpdateTabsPosition();
}

bool VerticalTabsTabWidget::event(QEvent* const event)
{
  if (event->type() == QEvent::StyleChange)
    UpdateTabsPosition();

  return QTabWidget::event(event);
}

void VerticalTabsTabWidget::UpdateTabsPosition()
{
  // If the application style customizes the QTabBar, vertical tabs are not supported. QMacStyle
  // does not support it either. The regular horizontal tabs will be used in those cases.
  const bool custom_tab_bar{qApp->styleSheet().contains(QStringLiteral("QTabBar")) ||
                            qApp->style()->name() == QStringLiteral("macOS")};
  static_cast<VerticalTabBar*>(tabBar())->UseVerticalTabs(!custom_tab_bar);
  setTabPosition(custom_tab_bar ? QTabWidget::TabPosition::North : QTabWidget::TabPosition::West);
}

}  // namespace QtUtils
