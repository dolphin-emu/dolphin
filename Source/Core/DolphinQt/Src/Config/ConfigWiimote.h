#include <QWidget>

class DConfigWiimote : public QWidget
{
	Q_OBJECT

public:
	DConfigWiimote(QWidget* parent = NULL);

public slots:
	void Reset();
	void Apply();
};
