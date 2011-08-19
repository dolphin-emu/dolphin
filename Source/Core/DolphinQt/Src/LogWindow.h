#include <QDockWidget>
#include <QVector>

#include "LogManager.h"

class QPlainTextEdit;
class QCheckBox;
class QStandardItemModel;
class DLogSettingsDock : public QDockWidget
{
	Q_OBJECT

public:
	DLogSettingsDock(QWidget * parent = NULL, Qt::WindowFlags flags = 0);
	~DLogSettingsDock();

signals:
	void Closed();

protected:
	void closeEvent(QCloseEvent* event);

private slots:
	void OnToggleChannels();

private:
	QVector<QCheckBox*> logChannels;
	QStandardItemModel* channelModel;
	bool enableAllChannels;
};

class DLogWindow : public QDockWidget, LogListener
{
	Q_OBJECT

public:
	DLogWindow(QWidget * parent = NULL, Qt::WindowFlags flags = 0);
	~DLogWindow();

signals:
	void Closed();

protected:
	void closeEvent(QCloseEvent* event);

private:
	QPlainTextEdit* logEdit;

	// LogListener
	void Log(LogTypes::LOG_LEVELS, const char *msg);
	const char *getName() const { return "LogWindow"; }
};
