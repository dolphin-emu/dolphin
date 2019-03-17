// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/SearchBar.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>

SearchBar::SearchBar(QWidget* parent) : QWidget(parent)
{
  CreateWidgets();
  ConnectWidgets();

  setFixedHeight(32);

  setHidden(true);

  installEventFilter(this);
}

void SearchBar::CreateWidgets()
{
  m_search_edit = new QLineEdit;
  m_close_button = new QPushButton(tr("Close"));

  m_search_edit->setPlaceholderText(tr("Search games..."));

  auto* layout = new QHBoxLayout;

  layout->addWidget(m_search_edit);
  layout->addWidget(m_close_button);
  layout->setSizeConstraint(QLayout::SetMinAndMaxSize);

  setLayout(layout);
}

void SearchBar::Show()
{
  m_search_edit->setFocus();
  m_search_edit->selectAll();

  // Re-apply the filter string.
  emit Search(m_search_edit->text());

  show();
}

void SearchBar::Hide()
{
  // Clear the filter string.
  emit Search(QString());

  m_search_edit->clearFocus();

  hide();
}

void SearchBar::ConnectWidgets()
{
  connect(m_search_edit, &QLineEdit::textChanged, this, &SearchBar::Search);
  connect(m_close_button, &QPushButton::pressed, this, &SearchBar::Hide);
}

bool SearchBar::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::KeyPress)
  {
    if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape)
      Hide();
  }

  return false;
}
