// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QEvent>
#include <string>

enum HostEvent {
	TitleEvent = QEvent::User + 1,
};

class HostTitleEvent final : public QEvent
{
public:
	HostTitleEvent(const std::string& title);
	const std::string m_title;
};
