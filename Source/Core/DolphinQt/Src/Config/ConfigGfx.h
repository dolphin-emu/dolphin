#include <QTabWidget>

class DConfigGfx : public QTabWidget
{
	Q_OBJECT

public:
	DConfigGfx(QWidget* parent = NULL);

public slots:
	void Reset();
	void Apply();
};
