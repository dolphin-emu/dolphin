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

DLogWindow::DLogWindow(const QString & title, QWidget * parent, Qt::WindowFlags flags) : QDockWidget(title, parent, flags)
{
	// Create widgets
	logManager = LogManager::GetInstance();

	QComboBox* fontBox = new QComboBox;
	QStringList fontList;
	fontList << tr("Default font") << tr("Monospaced font") << tr("Selected font");
	fontBox->addItems(fontList);

	QCheckBox* wordWrapCB = new QCheckBox(tr("Word wrap"));
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
	QPushButton* clearButton = new QPushButton(tr("Clear"));

	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
		logChannels.push_back(new QCheckBox(QString(logManager->getFullName( (LogTypes::LOG_TYPE)i ))));

	// Create left layout
	DLayoutWidgetV* leftLayout = new DLayoutWidgetV;
	leftLayout->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

	DGroupBoxV* verbosityGroup = new DGroupBoxV(tr("Verbosity"));
	for (int i = 0; i < MAX_LOGLEVEL && i < logLevels.size(); ++i)
        verbosityGroup->addWidget(logLevelsRadio[i]);
	leftLayout->addWidget(verbosityGroup);

	DGroupBoxV* optionsGroup = new DGroupBoxV(tr("Options"));
	optionsGroup->addWidget(fontBox);
	optionsGroup->addWidget(wordWrapCB);
	optionsGroup->addWidget(writeToFileCB);
	optionsGroup->addWidget(writeToConsoleCB);
	optionsGroup->addWidget(writeToWindowCB);
	leftLayout->addWidget(optionsGroup);

	QHBoxLayout* buttonLayout = new QHBoxLayout;
	buttonLayout->addWidget(toggleAllButton);
	buttonLayout->addWidget(clearButton);
	leftLayout->addLayout(buttonLayout);

	QScrollArea* channelArea = new QScrollArea;
	QTreeView* channelList = new QTreeView;
	QStandardItemModel* channelModel = new QStandardItemModel;
	channelModel->setRowCount(LogTypes::NUMBER_OF_LOGS);
	channelList->setRootIsDecorated(false);
	channelList->setAlternatingRowColors(true);
	channelList->setEditTriggers(QAbstractItemView::NoEditTriggers);
	channelList->setUniformRowHeights(true);
	channelList->setHeaderHidden(true);
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; i++)
	{
		QStandardItem* item = new QStandardItem(QString(logManager->getFullName( (LogTypes::LOG_TYPE)i))); // TODO: tr?
		item->setCheckable(true);
		channelModel->setItem(i, item);
	}
//	channelList->resizeColumnToContents(0);
	channelList->header()->setResizeMode(QHeaderView::Stretch);
	channelList->setModel(channelModel);
	channelArea->setWidget(channelList);
	leftLayout->addWidget(channelArea);

	// Create right layout
	logEdit = new QPlainTextEdit;
	logEdit->setReadOnly(true);
	logEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	// Create top layout
	DLayoutWidgetH* topLayout = new DLayoutWidgetH;
	topLayout->addWidget(leftLayout);
	topLayout->addWidget(logEdit);
	setWidget(topLayout);

	// setup other stuff
	// TODO: Check if logging is actually enabled
	// TODO: Some messages get dropped in the beginning
	for (int i = 0; i < LogTypes::NUMBER_OF_LOGS; ++i)
	{
		logManager->addListener((LogTypes::LOG_TYPE)i, this);
	}
}

DLogWindow::~DLogWindow()
{

}

void DLogWindow::Log(LogTypes::LOG_LEVELS, const char *msg)
{
	logEdit->appendPlainText(QString(msg));
}

void DLogWindow::closeEvent(QCloseEvent* event)
{
	emit Closed();
}
