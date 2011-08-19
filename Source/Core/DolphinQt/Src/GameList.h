#pragma once

#include <QStandardItemModel>
#include <QTableView>
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
	virtual ~DAbstractGameList() {};

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
	virtual ~DGameList();

	void ScanForIsos();
	GameListItem* GetSelectedISO();

protected:
	void RefreshView();

private:
	void mouseDoubleClickEvent(QMouseEvent*);

	QStandardItemModel* sourceModel;

	DAbstractGameList abstrGameList;

signals:
	void StartGame();
};

class DGameTable : public QTableView
{
	Q_OBJECT

public:
	DGameTable(DAbstractProgressBar* progressBar);
	virtual ~DGameTable();

	void ScanForIsos();
	GameListItem* GetSelectedISO();

protected:
	void RefreshView();
	void resizeEvent(QResizeEvent*);

private:
	void mouseDoubleClickEvent(QMouseEvent*);

	QStandardItemModel* sourceModel;

	DAbstractGameList abstrGameList;

	unsigned int num_columns;

	QMap<u8*,QPixmap> pixmap_cache;

signals:
	void StartGame();
};
