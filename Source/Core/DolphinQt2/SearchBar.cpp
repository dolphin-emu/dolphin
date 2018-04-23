// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt2/SearchBar.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

SearchBar::SearchBar(QWidget* parent) : QWidget(parent)
{
  CreateWidgets();
  ConnectWidgets();

  setFixedHeight(32);

  setHidden(true);
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

void SearchBar::Toggle()
{
  m_search_edit->clear();

  setHidden(isVisible());

  if (isVisible())
    m_search_edit->setFocus();
  else
    m_search_edit->clearFocus();
}

void SearchBar::ConnectWidgets()
{
  connect(m_search_edit, &QLineEdit::textChanged, this, &SearchBar::Search);
  connect(m_close_button, &QPushButton::pressed, this, &SearchBar::Toggle);
}
