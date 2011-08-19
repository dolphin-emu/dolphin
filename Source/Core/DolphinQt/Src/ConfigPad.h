#include <QTabWidget>

class DConfigPadWidget : public QTabWidget
{
	Q_OBJECT

public:
    DConfigPadWidget(QWidget* parent = NULL);
    virtual ~DConfigPadWidget();

public slots:
	void Reset();
	void Apply();
};
