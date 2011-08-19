#pragma once

#include <QStandardItemModel>
#include <QTreeView>
#include "ISOFile.h"

#include "GameList.h"

class ProgressDialogCallbackClass
{
public:
	ProgressDialogCallbackClass() {}
	~ProgressDialogCallbackClass() {}
	virtual void* OnIsoScanStarted(int numItems) { return  NULL; };
	virtual void OnIsoScanProgress(void* dialogptr, int item, std::string nowScanning) {};
	virtual void OnIsoScanFinished(void* dialogptr) {};
};

class DAbstractGameList
{
public:
	DAbstractGameList() {};
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

	void Rescan(ProgressDialogCallbackClass* callback);

	std::vector<GameListItem>& getItems() { return items; }

private:
	std::vector<GameListItem> items;
};

class DGameList : public QTreeView, ProgressDialogCallbackClass
{
	Q_OBJECT

public:
	DGameList();
	~DGameList();

public:
	void* OnIsoScanStarted(int numItems);
	void OnIsoScanProgress(void* dialogptr, int item, std::string nowScanning);
	void OnIsoScanFinished(void* dialogptr);

	void ScanForIsos();
	void RebuildList();

	QString GetSelectedFilename();

private:
	DAbstractGameList abstrGameList;

	QStandardItemModel* sourceModel;
};
