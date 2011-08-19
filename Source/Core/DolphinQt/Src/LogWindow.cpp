#include <QtGui>
#include "LogWindow.h"
#include "Util.h"

#include "LogManager.h"

class DCheckBoxList : public QTreeView
{
public:
	DCheckBoxList(QWidget* parent = NULL) : QTreeView(parent)
	{
//		setFlags(Qt::ItemIsUserCheckable);
		setModel(model = new QStandardItemModel);
	    setRootIsDecorated(false);
	    setAlternatingRowColors(true);
	    setEditTriggers(QAbstractItemView::NoEditTriggers);
	    setUniformRowHeights(true);
	}

	void add(int row, QString text)
	{
		model->setItem(row, new QStandardItem(text));
	}

private:
	QStandardItemModel* model;
};

DLogSettingsDock::DLogSettingsDock(QWidget * parent, Qt::WindowFlags flags)
		: QDockWidget(tr("Log Settings"), parent, flags)
		, enableAllChannels(true)
{
	LogManager* logManager = LogManager::GetInstance();

	QCheckBox* writeToFileCB = new QCheckBox(tr("Write to file"));
	QCheckBox* writeToConsoleCB = new QCheckBox(tr("Write to console"));
	QCheckBox* writeToWindowCB = new QCheckBox(tr("Write to window ->"));

	QStringList logLevels;
	logLevels << tr("Notice") << tr("Error") << tr("Warning") << tr("Info") << tr("Debug");
	QRadioButton* logLevelsRadio[MAX_LOGLEVEL];
	for (int i = 0; i < MAX_LOGLEVEL && i < logLevels.size(); ++i)
        logLevelsRadio[i] = new QRadioButton(logLevels.at(i));
	logLevelsRadio[0]->setChecked(true);

	QPushButton* toggleAllButton = new QPushButton(tr("Toggle all"));
	connect(toggleAllButton, SIGNAL(clicked()), this, SLOT(OnToggleChannels()));

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		logChannels.push_back(new QCheckBox(QString(logManager->GetFullName( (LogTypes::LOG_TYPE)i ))));

	// Create left layout
	DLayoutWidgetV* leftLayout = new DLayoutWidgetV;
	leftLayout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

	DGroupBoxV* verbosityGroup = new DGroupBoxV(tr("Verbosity"));
	for (int i = 0; i < MAX_LOGLEVEL && i < logLevels.size(); ++i)
        verbosityGroup->addWidget(logLevelsRadio[i]);
	leftLayout->addWidget(verbosityGroup);

	DGroupBoxV* optionsGroup = new DGroupBoxV(tr("Options"));
	optionsGroup->addWidget(writeToFileCB);
	optionsGroup->addWidget(writeToConsoleCB);
	optionsGroup->addWidget(writeToWindowCB);
	leftLayout->addWidget(optionsGroup);

	leftLayout->addWidget(toggleAllButton);

	QScrollArea* channelArea = new QScrollArea;
	QTreeView* channelList = new QTreeView;
	channelModel = new QStandardItemModel;
	channelModel->setRowCount(LogTypes::NUMBER_OF_LOGS);
	channelList->setRootIsDecorated(false);
	channelList->setAlternatingRowColors(true);
	channelList->setEditTriggers(QAbstractItemView::NoEditTriggers);
	channelList->setUniformRowHeights(true);
	channelList->setHeaderHidden(true);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		// TODO: Scrollarea doesn't fit this widget, yet...
		QStandardItem* item = new QStandardItem(QString(logManager->GetFullName( (LogTypes::LOG_TYPE)i))); // TODO: tr?
		item->setCheckable(true);
		channelModel->setItem(i, item);
	}
//	channelList->resizeColumnToContents(0);
	channelList->header()->setResizeMode(QHeaderView::Stretch);
	channelList->setModel(channelModel);
	channelArea->setWidget(channelList);
	leftLayout->addWidget(channelArea);
	setWidget(leftLayout);
}

DLogSettingsDock::~DLogSettingsDock()
{

}

void DLogSettingsDock::OnToggleChannels()
{
	// if no channels are selected, but enableAllChannels is false, force enableAllChannels to true (and vice versa)
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		if (channelModel->item(i)->checkState() != (enableAllChannels ? Qt::Checked : Qt::Unchecked))
		{
			break;
		}
		else if (i == LogTypes::NUMBER_OF_LOGS-1)
		{
			enableAllChannels = !enableAllChannels;
		}
	}

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
		channelModel->item(i)->setCheckState(enableAllChannels ? Qt::Checked : Qt::Unchecked);

	enableAllChannels = !enableAllChannels;
}

void DLogSettingsDock::closeEvent(QCloseEvent* event)
{
	// TODO: Handle this differently, since we need to keep the window open if the "really stop emulation?" question was answered with "no"
	emit Closed();
	QWidget::closeEvent(event);
}

DLogWindow::DLogWindow(QWidget * parent, Qt::WindowFlags flags)
		: QDockWidget(tr("Logging"), parent, flags)
{
	LogManager* logManager = LogManager::GetInstance();

	// Top layout widgets
	QComboBox* fontBox = new QComboBox;
	QStringList fontList;
	fontList << tr("Default font") << tr("Monospaced font") << tr("Selected font");
	fontBox->addItems(fontList);

	QCheckBox* wordWrapCB = new QCheckBox(tr("Word wrap"));
	QPushButton* clearButton = new QPushButton(tr("Clear"));

	// Bottom layout widget
	logEdit = new QPlainTextEdit;
	logEdit->setReadOnly(true);
	logEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	// Create layouts
	DLayoutWidgetH* topLayout = new DLayoutWidgetH;
	topLayout->addWidget(clearButton);
	topLayout->addWidget(fontBox);
	topLayout->addWidget(wordWrapCB);

	DLayoutWidgetV* layout = new DLayoutWidgetV;
	layout->addWidget(topLayout);
	layout->addWidget(logEdit);
	setWidget(layout);

	// setup other stuff
	// TODO: Check if logging is actually enabled
	// TODO: Some messages get dropped in the beginning
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		logManager->AddListener((LogTypes::LOG_TYPE)i, this);
	}
}

DLogWindow::~DLogWindow()
{

}

void DLogWindow::Log(LogTypes::LOG_LEVELS, const char *msg)
{
	// TODO: For some reason, this line makes the emulator crash after a few log messages..
	// logEdit->appendPlainText(QString(msg));
}

void DLogWindow::closeEvent(QCloseEvent* event)
{
	emit Closed();
	QWidget::closeEvent(event);
}
