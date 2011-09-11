#pragma once

#include <QStandardItemModel>
#include <QTableView>
#include <QTreeView>
#include "ISOFile.h"

class QGridLayout;
class AbstractProgressBar
{
public:
	virtual void SetValue(int value, std::string FileName) = 0;
	virtual void SetRange(int min, int max) = 0;
	virtual void SetVisible(bool visible) = 0;
};

class AbstractGameBrowser
{
public:
	AbstractGameBrowser(AbstractProgressBar* progBar) : progressBar(progBar) {};
	virtual ~AbstractGameBrowser() {};

	enum { COLUMN_PLATFORM = 0, COLUMN_BANNER, COLUMN_TITLE, COLUMN_NOTES,
			COLUMN_COUNTRY, COLUMN_SIZE, COLUMN_EMULATION_STATE, NUM_COLUMNS };

	void Rescan();

	const std::vector<GameListItem>& getItems() const { return items; }

private:
	std::vector<GameListItem> items;
	AbstractProgressBar* progressBar;
};

// TODO: Add a game list which acts like a file explorer

class DGameListProgressBar;
class GameBrowserImpl
{
public:
	virtual GameListItem const* GetSelectedISO() const = 0;
	virtual void RefreshView() = 0;
};

class DGameBrowser : public QWidget
{
	Q_OBJECT

public:
	typedef enum { Style_List, Style_Grid } Style;

	DGameBrowser(Style initialStyle, QWidget* parent = NULL);
	~DGameBrowser();

	Style GetStyle();
	void SetStyle(Style layout);
	void ScanForIsos();
	GameListItem const* GetSelectedISO() const;

private:
	DGameListProgressBar* progBar;
	AbstractGameBrowser abstrGameBrowser;
	GameBrowserImpl* gameBrowser;
	Style style;

	QGridLayout* mainLayout;

signals:
	void StartGame();
};

// TODO: These should be in a private header
class DGameList : public QTreeView, public GameBrowserImpl
{
	Q_OBJECT

public:
	DGameList(const AbstractGameBrowser& abstrGameList);
	virtual ~DGameList();

	GameListItem const* GetSelectedISO() const;
	void RefreshView();

private slots:
	void OnItemActivated(const QModelIndex&);

private:
	QStandardItemModel* sourceModel;
	const AbstractGameBrowser& abstrGameBrowser;

signals:
	void StartGame();
};

class DGameTable : public QTableView, public GameBrowserImpl
{
	Q_OBJECT

public:
	DGameTable(const AbstractGameBrowser& abstrGameList);
	virtual ~DGameTable();

	void ScanForIsos();
	GameListItem const* GetSelectedISO() const;

private slots:
	void OnItemActivated(const QModelIndex&);

protected:
	void RefreshView();
	void RebuildGrid();
	void resizeEvent(QResizeEvent*);

private:
	const AbstractGameBrowser& abstrGameBrowser;

	QStandardItemModel* sourceModel;
	unsigned int num_columns;

	QMap<u8 const*,QPixmap> pixmap_cache; // needed, since resizing will lag too much without caching

signals:
	void StartGame();
};
