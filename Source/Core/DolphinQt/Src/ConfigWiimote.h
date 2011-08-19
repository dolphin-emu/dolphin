#include <QWidget>

class DConfigWiimote : public QWidget
{
	Q_OBJECT

public:
    DConfigWiimote(QWidget* parent = NULL);
    virtual ~DConfigWiimote();

public slots:
	void Reset();
	void Apply();
};
