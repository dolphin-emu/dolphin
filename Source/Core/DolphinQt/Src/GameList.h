#pragma once

#include <QStandardItemModel>
#include <QTreeView>
#include "ISOFile.h"

class DAbstractProgressBar
{
public:
	DAbstractProgressBar() {}
	virtual ~DAbstractProgressBar() {}

	virtual void SetValue(int value) = 0;
	virtual void SetRange(int min, int max) = 0;
	virtual void SetLabel(std::string str) = 0;
	virtual void SetVisible(bool visible) = 0;
};

class DAbstractGameList
{
public:
	DAbstractGameList(DAbstractProgressBar* progBar) : progressBar(progBar) {};
	~DAbstractGameList() {};

	enum
	{
		COLUMN_PLATFORM = 0,
		COLUMN_BANNER,
		COLUMN_TITLE,
		COLUMN_NOTES,
		COLUMN_COUNTRY,
		COLUMN_SIZE,
		COLUMN_EMULATION_STATE,
		NUM_COLUMNS
	};

	void Rescan();

	std::vector<GameListItem>& getItems() { return items; }

private:
	std::vector<GameListItem> items;
	DAbstractProgressBar* progressBar;
};

class DGameList : public QTreeView
{
	Q_OBJECT

public:
	DGameList(DAbstractProgressBar* progressBar);
	~DGameList();

public:
	void RebuildList();

	GameListItem* GetSelectedISO();

	void ScanForIsos();

private:

	void mouseDoubleClickEvent (QMouseEvent*);

	DAbstractGameList abstrGameList;

	QStandardItemModel* sourceModel;

signals:
	void DoubleLeftClicked();
};
