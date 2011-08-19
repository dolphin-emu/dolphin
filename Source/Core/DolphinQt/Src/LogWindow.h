#include <QDockWidget>
#include <QVector>

#include "LogManager.h"

class QPlainTextEdit;
class QCheckBox;
class DLogWindow : public QDockWidget, LogListener
{
	Q_OBJECT

public:
	DLogWindow(const QString & title, QWidget * parent = NULL, Qt::WindowFlags flags = 0);
	~DLogWindow();

signals:
	void Closed();

protected:
	void closeEvent(QCloseEvent* event);

private:
	LogManager* logManager;

	QPlainTextEdit* logEdit;
	QVector<QCheckBox*> logChannels;

	// LogListener
	void Log(LogTypes::LOG_LEVELS, const char *msg);
	const char *getName() const { return "LogWindow"; }
};
